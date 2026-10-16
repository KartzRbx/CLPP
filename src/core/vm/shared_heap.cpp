#include "clpp/value.hpp"
#include "clpp/numeric.hpp"
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace clpp {
namespace {
// Trivial bootstrap state remains valid for native Value roots created during static teardown.
constinit std::atomic_flag arena_cache_lock = ATOMIC_FLAG_INIT;
HeapArena* cached_arena = nullptr; // non-owning; destruction unregisters before releasing any fields
struct ArenaCacheGuard {
  ArenaCacheGuard() {
    while (arena_cache_lock.test_and_set(std::memory_order_acquire)) std::this_thread::yield();
  }
  ~ArenaCacheGuard() { arena_cache_lock.clear(std::memory_order_release); }
  ArenaCacheGuard(const ArenaCacheGuard&) = delete;
  ArenaCacheGuard& operator=(const ArenaCacheGuard&) = delete;
};
}
struct HeapObject {
  std::weak_ptr<HeapArena> arena;
  std::vector<Value> fields;
  bool frozen{false};
  std::shared_ptr<const BytecodeChunk> code;
};
struct HeapArena {
  std::weak_ptr<HeapArena> self;
  std::recursive_mutex mutex;
  std::vector<std::shared_ptr<HeapObject>> objects;
  std::size_t allocations_since_collection{0};
  std::size_t collection_budget{512};
  ~HeapArena() {
    {
      const ArenaCacheGuard guard;
      if (cached_arena == this) cached_arena = nullptr;
    }
    // The last external root is gone. Break every interior edge before dropping arena ownership.
    for (const auto& object : objects) object->fields.clear();
  }
  std::size_t collect() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::unordered_map<HeapObject*, std::size_t> incoming;
    for (const auto& object : objects) {
      incoming.try_emplace(object.get(), 0);
      for (const auto& value : object->fields) if (value.m_object) ++incoming[value.m_object.get()];
    }
    std::unordered_set<HeapObject*> live;
    std::vector<HeapObject*> work;
    for (const auto& object : objects) {
      if (static_cast<std::size_t>(object.use_count()) > 1 + incoming[object.get()]) work.push_back(object.get());
    }
    while (!work.empty()) {
      auto* object = work.back(); work.pop_back();
      if (!live.insert(object).second) continue;
      for (const auto& value : object->fields) if (value.m_object) work.push_back(value.m_object.get());
    }
    const auto before = objects.size();
    // Arena ownership prevents recursive destruction while clearing unreachable cyclic edges.
    for (const auto& object : objects) if (!live.contains(object.get())) object->fields.clear();
    objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const auto& object) {
      return !live.contains(object.get());
    }), objects.end());
    allocations_since_collection = 0;
    collection_budget = std::max(std::size_t{512}, objects.size());
    return before - objects.size();
  }
};
namespace {
std::shared_ptr<HeapArena> active_arena(bool create) {
  const ArenaCacheGuard guard;
  auto owner = cached_arena != nullptr ? cached_arena->self.lock() : nullptr;
  if (!owner && create) {
    owner = std::make_shared<HeapArena>();
    owner->self = owner;
    cached_arena = owner.get();
  }
  return owner;
}
void writable(const HeapObject& object) { if (object.frozen) throw HeapError("cannot mutate a frozen value"); }
}
HeapStats shared_heap_stats() {
  auto owner = active_arena(false); if (!owner) return {};
  std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  HeapStats result{owner->objects.size(), 0};
  for (const auto& object : owner->objects) if (object->frozen) ++result.frozen;
  return result;
}
std::size_t collect_shared_heap() { auto owner = active_arena(false); return owner ? owner->collect() : 0; }
std::shared_ptr<HeapArena> Value::arena() const {
  auto owner = m_arena ? m_arena : m_object ? m_object->arena.lock() : nullptr;
  if (!owner || !m_object) throw HeapError("invalid heap reference");
  return owner;
}
void Value::reset_reference() {
  auto owner = m_arena ? m_arena : m_object ? m_object->arena.lock() : nullptr;
  if (owner) { std::lock_guard<std::recursive_mutex> lock(owner->mutex); m_object.reset(); m_arena.reset(); }
  else { m_object.reset(); m_arena.reset(); }
}
Value::~Value() { reset_reference(); }
void Value::move_slow(Value& other) noexcept {
  reset_reference();
  auto owner = other.m_arena ? other.m_arena : other.m_object ? other.m_object->arena.lock() : nullptr;
  const auto move = [&] {
    kind=other.kind; dims=other.dims; number=other.number; integer=other.integer;
    y=other.y; z=other.z; w=other.w; text=std::move(other.text); table=other.table;
    m_object=std::move(other.m_object); m_arena=std::move(other.m_arena);
    other.kind=Kind::Number; other.number=0; other.table=nullptr;
  };
  if (owner) { std::lock_guard<std::recursive_mutex> lock(owner->mutex); move(); } else move();
}
void Value::assign_slow(const Value& other) {
  if (this == &other) return;
  reset_reference();
  kind=other.kind; dims=other.dims; number=other.number; integer=other.integer;
  y=other.y; z=other.z; w=other.w; text=other.text; table=other.table;
  if (other.m_object) {
    auto owner=other.arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
    m_object=other.m_object; m_arena=std::move(owner);
  }
}
void Value::clear_payload() {
  reset_reference(); kind=Kind::Number; y=z=w=0; text.clear(); table=nullptr;
}
Value Value::interior(const Value& value) { Value result=value; result.m_arena.reset(); return result; }
Value Value::make_fields(std::vector<Value> values, Kind type) {
  auto owner=active_arena(true); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  Value result; result.kind=type; result.m_arena=owner; result.m_object=std::make_shared<HeapObject>();
  result.m_object->arena = owner;
  result.m_object->fields = std::move(values);
  // Adopt the constructor's storage without duplicating large arrays; interior edges retain nodes only.
  for (auto& value : result.m_object->fields) value.m_arena.reset();
  owner->objects.push_back(result.m_object);
  if (++owner->allocations_since_collection >= owner->collection_budget) owner->collect();
  return result;
}
Value Value::struct_of(std::vector<Value> values) { return make_fields(std::move(values), Kind::Struct); }
Value Value::cell_of(Value value) { return make_fields({std::move(value)}, Kind::Cell); }
Value Value::signal_of() { return make_fields({}, Kind::Signal); }
Value Value::connection_of(Value callback, bool once, bool managed) {
  return make_fields({Value::integer_of(1), std::move(callback), Value::integer_of(once ? 1 : 0), Value::integer_of(managed ? 1 : 0)}, Kind::Connection);
}
Value Value::subscribe(Value callback, bool once, bool managed) {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Signal && kind != Kind::Cell) throw HeapError("invalid subscription owner");
  writable(*m_object);
  auto connection = connection_of(std::move(callback), once, managed);
  auto& fields = m_object->fields;
  const auto first = fields.begin() + (kind == Kind::Cell ? 1 : 0);
  fields.erase(std::remove_if(first, fields.end(), [](const Value& value) { return !value.connected(); }), fields.end());
  fields.push_back(interior(connection));
  return connection;
}
std::vector<Value> Value::connections() {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Signal && kind != Kind::Cell) throw HeapError("invalid subscription owner");
  auto& fields = m_object->fields;
  const auto first = fields.begin() + (kind == Kind::Cell ? 1 : 0);
  fields.erase(std::remove_if(first, fields.end(), [](const Value& value) { return !value.connected(); }), fields.end());
  return {fields.begin() + (kind == Kind::Cell ? 1 : 0), fields.end()};
}
bool Value::connected() const {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Connection || m_object->fields.size() != 4) throw HeapError("invalid connection");
  return m_object->fields[0].integer != 0;
}
Value Value::take_callback() {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Connection || m_object->fields.size() != 4) throw HeapError("invalid connection");
  if (m_object->fields[0].integer == 0) return Value::null_of();
  Value callback = m_object->fields[1];
  if (m_object->fields[2].integer != 0) {
    writable(*m_object); m_object->fields[0] = Value::integer_of(0); m_object->fields[1] = Value::null_of();
  }
  return callback;
}
void Value::disconnect() {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Connection || m_object->fields.size() != 4) throw HeapError("invalid connection");
  if (m_object->fields[0].integer == 0) return;
  writable(*m_object); m_object->fields[0] = Value::integer_of(0); m_object->fields[1] = Value::null_of();
}
void Value::clear_managed() {
  auto owner = arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (kind != Kind::Signal) throw HeapError("invalid signal");
  writable(*m_object);
  for (const auto& connection : m_object->fields) {
    if (connection.kind != Kind::Connection || !connection.m_object) throw HeapError("invalid connection");
    if (connection.field(3).integer != 0 && connection.connected()) writable(*connection.m_object);
  }
  for (auto& connection : m_object->fields) if (connection.field(3).integer != 0) connection.disconnect();
  auto& fields = m_object->fields;
  fields.erase(std::remove_if(fields.begin(), fields.end(), [](const auto& value) { return !value.connected(); }), fields.end());
}
Value Value::closure_of(std::uint16_t function, std::vector<Value> cells, std::shared_ptr<const BytecodeChunk> code) {
  auto result = make_fields(std::move(cells), Kind::Closure);
  result.integer = function; result.m_object->code = std::move(code); return result;
}
std::shared_ptr<const BytecodeChunk> Value::closure_code() const {
  return is_closure() && m_object ? m_object->code : nullptr;
}
Value Value::task_fields(std::vector<Value> values) { return make_fields(std::move(values), Kind::Task); }
Value Value::task_fields(std::vector<Value> values, std::shared_ptr<const BytecodeChunk> code) {
  auto result = task_fields(std::move(values)); result.m_object->code = std::move(code); return result;
}
std::shared_ptr<const BytecodeChunk> Value::task_code() const {
  return is_task() && m_object ? m_object->code : nullptr;
}
Value Value::task_of(Value inner) { auto result=task_fields({std::move(inner)}); result.y=2; return result; }
std::size_t Value::field_count() const {
  if (!m_object) return 0;
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); return m_object->fields.size();
}
Value Value::field(std::size_t index) const {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  if (index >= m_object->fields.size()) throw HeapError("index out of range");
  return m_object->fields[index];
}
std::vector<Value> Value::snapshot_fields() const {
  if (!m_object) return {};
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); return m_object->fields;
}
void Value::set_field(std::size_t index, const Value& value) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  if (index >= m_object->fields.size()) throw HeapError("index out of range");
  m_object->fields[index]=interior(value);
}
void Value::append_field(const Value& value) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  m_object->fields.push_back(interior(value));
}
void Value::insert_field(std::size_t index, const Value& value) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  if (index > m_object->fields.size()) throw HeapError("index out of range");
  m_object->fields.insert(m_object->fields.begin()+static_cast<std::ptrdiff_t>(index),interior(value));
}
Value Value::remove_field(std::size_t index) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  if (index >= m_object->fields.size()) throw HeapError("index out of range");
  Value result=m_object->fields[index];
  m_object->fields.erase(m_object->fields.begin()+static_cast<std::ptrdiff_t>(index)); return result;
}
Value Value::pop_field() {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  if (m_object->fields.empty()) throw HeapError("pop from an empty list");
  Value result=m_object->fields.back(); m_object->fields.pop_back(); return result;
}
bool Value::dictionary_get(const Value& key, Value& out) const {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  for (std::size_t i=0; i+1<m_object->fields.size(); i+=2) if (m_object->fields[i].same_value(key)) { out=m_object->fields[i+1]; return true; }
  return false;
}
void Value::dictionary_set(const Value& key, const Value& value) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); writable(*m_object);
  for (std::size_t i=0; i+1<m_object->fields.size(); i+=2) if (m_object->fields[i].same_value(key)) { m_object->fields[i+1]=interior(value); return; }
  m_object->fields.reserve(m_object->fields.size()+2);
  m_object->fields.push_back(interior(key)); m_object->fields.push_back(interior(value));
}
bool Value::dictionary_remove(const Value& key, Value& out) {
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  const auto original = m_object; // out can be the receiver itself; retain its original storage
  writable(*original);
  for (std::size_t i=0; i+1<original->fields.size(); i+=2) if (original->fields[i].same_value(key)) {
    out=original->fields[i+1]; original->fields.erase(original->fields.begin()+static_cast<std::ptrdiff_t>(i),original->fields.begin()+static_cast<std::ptrdiff_t>(i+2)); return true;
  }
  return false;
}
void Value::freeze() const {
  if ((!is_struct() && !is_resource()) || !m_object) return;
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex);
  std::unordered_set<HeapObject*> visited; std::vector<HeapObject*> work{m_object.get()};
  while (!work.empty()) {
    auto* object=work.back(); work.pop_back(); if (!visited.insert(object).second) continue;
    for (const auto& value : object->fields) if ((value.is_struct() || value.is_resource()) && value.m_object) work.push_back(value.m_object.get());
  }
  for (auto* object : visited) object->frozen=true;
}
bool Value::frozen() const {
  if (!m_object) return false;
  auto owner=arena(); std::lock_guard<std::recursive_mutex> lock(owner->mutex); return m_object->frozen;
}
const void* Value::identity() const { return m_object.get(); }
bool Value::same_value(const Value& other) const {
  if (is_number() && other.is_number()) {
    if (is_integer() && other.is_integer()) return integer==other.integer;
    if (is_integer()) return numeric::compare(integer,other.number)==0;
    if (other.is_integer()) return numeric::compare(other.integer,number)==0;
    return number==other.number;
  }
  if (kind!=other.kind) return false;
  if (is_null()) return true;
  if (is_struct() || is_task() || is_closure() || is_cell() || is_resource()) return identity()==other.identity();
  if (is_string()) return text==other.text;
  if (is_vector()) return dims==other.dims && number==other.number && y==other.y && z==other.z && w==other.w;
  return table==other.table && number==other.number;
}
} // namespace clpp
