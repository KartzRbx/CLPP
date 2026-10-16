#include <bit>
#include "clpp/numeric.hpp"
#include "clpp/vm.hpp"

#include "clpp/register_ir.hpp"
#include "clpp/stdlib.hpp"
#include "core/vm/gc.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>
#include <cstring>
#include <mutex>
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace clpp {

namespace {

[[nodiscard]] std::optional<std::int64_t> integral_value(const Value& value) {
  if (value.is_integer()) return value.integer;
  if (!value.is_number() || !std::isfinite(value.number) || std::trunc(value.number) != value.number) return std::nullopt;
  return numeric::from_float(value.number);
}

[[nodiscard]] std::optional<std::size_t> array_index(const Value& value) {
  const auto integer = integral_value(value);
  if (!integer || *integer < 0 || static_cast<std::uint64_t>(*integer) > std::numeric_limits<std::size_t>::max()) return std::nullopt;
  return static_cast<std::size_t>(*integer);
}

[[nodiscard]] std::size_t utf8_width(const std::string& text, const std::size_t offset) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
  return std::min(width, text.size() - offset);
}

[[nodiscard]] std::string format_number(const double number) {
  // Whole numbers print as whole numbers (4499998500000, not 4.4999985e+12) as long as a double
  // holds them exactly; everything else prints the shortest text that reads back the same value.
  if (std::isfinite(number) && number == std::trunc(number) && std::fabs(number) < 9007199254740992.0) {
    return std::to_string(static_cast<long long>(number));
  }
  char buffer[64];
  const std::to_chars_result result =
      std::to_chars(buffer, buffer + sizeof(buffer), number, std::chars_format::general);
  if (result.ec != std::errc{}) {
    return {};
  }
  return std::string(buffer, result.ptr);
}

[[nodiscard]] std::string format_value(const Value& value, bool nested = false);

// post() shows lists as [1, 2] and dictionaries as {gold: 1}.
// struct values as (Ada, 100), vectors as (1, 2, 3). Strings inside a collection are quoted.
[[nodiscard]] std::string format_value(const Value& value, const bool nested) {
  if (value.is_boolean()) return value.number != 0 ? "true" : "false";
  static thread_local std::vector<const void*> path;
  struct Guard { std::vector<const void*>& path; bool active; ~Guard() { if (active) path.pop_back(); } };
  if (value.is_struct() && std::find(path.begin(), path.end(), value.identity()) != path.end()) return "<cycle>";
  if (path.size() >= 128) return "<depth limit>";
  const bool active = value.is_struct();
  if (active) path.push_back(value.identity());
  const Guard guard{path, active};

  if (value.is_null()) return "null";
  if (value.kind == Value::Kind::Signal) return "<signal>";
  if (value.kind == Value::Kind::Connection) return value.connected() ? "<connection>" : "<disconnected>";
  if (value.is_integer()) return std::to_string(value.integer);
  if (value.is_string()) {
    return nested ? "\"" + value.text + "\"" : value.text;
  }
  if (value.is_vector()) {
    std::string text = "(" + format_number(value.number) + ", " + format_number(value.y);
    if (value.dims >= 3) {
      text += ", " + format_number(value.z);
    }
    if (value.dims >= 4) {
      text += ", " + format_number(value.w);
    }
    return text + ")";
  }
  if (value.is_struct() && value.number == 65534 && value.field_count() == 1) {
    return format_value(value.field(0), nested);  // variant box
  }
  if (value.is_struct()) {
    const bool dictionary = value.number == 65535;
    const bool list = value.number == 0;
    std::string text = dictionary ? "{" : list ? "[" : "(";
    const std::size_t step = dictionary ? 2 : 1;
    for (std::size_t index = 0; index < value.field_count(); index += step) {
      if (index != 0) {
        text += ", ";
      }
      if (dictionary && index + 1 < value.field_count()) {
        text += format_value(value.field(index), false) + ": " + format_value(value.field(index + 1), true);
      } else {
        text += format_value(value.field(index), true);
      }
    }
    return text + (dictionary ? "}" : list ? "]" : ")");
  }
  return format_number(value.number);
}

[[nodiscard]] bool values_equal(const Value& left, const Value& right) { return left.same_value(right); }

[[nodiscard]] bool transferable_values(const std::vector<Value>& values) {
  std::vector<Value> pending = values;
  std::unordered_set<const void*> visited;
  while (!pending.empty()) {
    Value value = std::move(pending.back()); pending.pop_back();
    if (value.is_table()) return false; // legacy VM-local tables have no cross-VM ownership
    if ((value.is_struct() || value.is_task() || value.is_closure() || value.is_cell() || value.is_resource()) && visited.insert(value.identity()).second) {
      for (auto& child : value.snapshot_fields()) pending.push_back(std::move(child));
    }
  }
  return true;
}

[[nodiscard]] bool is_truthy(const Value& value) {
  if (value.is_null()) return false;
  if (value.is_string()) {
    return !value.text.empty();
  }
  return value.number != 0;
}

struct ThreadSlot {
  std::mutex mu;
  std::condition_variable cv;
  bool done{false};
  bool observed{false}; // a join/await/parallel consumer has received the result or failure
  SourceLocation error_location{0, 0};
  Value result{Value::number_of(0)};
  std::string error;
  std::thread worker;

  ThreadSlot() = default;
  ThreadSlot(const ThreadSlot&) = delete;
  ThreadSlot& operator=(const ThreadSlot&) = delete;
  // The worker lambda owns a reference to this slot. When the join side has already dropped
  // its reference, the last one dies on the worker thread itself, and a thread cannot join
  // itself (std::system_error "Resource deadlock avoided"). Detach in that case: the thread
  // is already finishing. Otherwise join, as std::jthread would (std::thread is used because
  // libc++ still ships jthread as experimental).
  ~ThreadSlot() {
    if (!worker.joinable()) {
      return;
    }
    if (worker.get_id() == std::this_thread::get_id()) {
      worker.detach();
    } else {
      worker.join();
    }
  }
};

} // namespace

struct AsyncState {
  Value task;
  std::unique_ptr<VirtualMachine> machine;
  RegChunk lowered;
  std::thread::id owner;
  bool started{false};
  bool running{false};
  std::atomic<bool> done{false};
  std::atomic<bool> observed{false};
  Value result;
  std::string error;
  SourceLocation error_location{0, 0};
};

struct RuntimeMutex {
  std::mutex mu;
  std::condition_variable changed;
  const void* owner{nullptr};
};

// One execution owns its concurrency resources. Workers retain this context while running.
struct RuntimeContext {
  std::mutex mu;
  std::mutex output_mu;
  std::vector<Value> globals;
  std::vector<bool> global_ready;
  std::vector<bool> global_const;
  std::vector<std::unique_ptr<std::atomic<long long>>> atomics;
  std::vector<std::unique_ptr<RuntimeMutex>> mutexes;
  // Destroy workers before the synchronization resources they can still use.
  std::vector<std::shared_ptr<ThreadSlot>> threads;
  std::vector<std::shared_ptr<AsyncState>> tasks;
  std::condition_variable changed;
  std::unordered_map<std::thread::id, std::size_t> schedulers;

  void drain() {
    std::size_t index = 0;
    while (true) {
      std::shared_ptr<ThreadSlot> slot;
      {
        std::lock_guard<std::mutex> lock(mu);
        if (index == threads.size()) break;
        slot = threads[index++];
      }
      // Joining a parent can register more descendants. Recheck the registry after each join.
      if (slot->worker.joinable()) slot->worker.join();
    }
  }

  std::pair<std::string, SourceLocation> unobserved_failure() {
    std::lock_guard<std::mutex> registry_lock(mu);
    // Inspect only after every worker has finished: later parents may consume an earlier failure.
    for (const auto& slot : threads) {
      std::lock_guard<std::mutex> slot_lock(slot->mu);
      if (!slot->observed && !slot->error.empty()) return {slot->error, slot->error_location};
    }
    return {};
  }
};

struct SchedulerScope {
  RuntimeContext& runtime;
  explicit SchedulerScope(RuntimeContext& context) : runtime(context) {
    std::lock_guard<std::mutex> lock(runtime.mu); ++runtime.schedulers[std::this_thread::get_id()];
  }
  ~SchedulerScope() {
    std::lock_guard<std::mutex> lock(runtime.mu);
    auto found = runtime.schedulers.find(std::this_thread::get_id());
    if (found != runtime.schedulers.end() && --found->second == 0) runtime.schedulers.erase(found);
    runtime.changed.notify_all();
  }
};

namespace {

[[nodiscard]] long long make_atomic(RuntimeContext& runtime, const long long initial) {
  std::lock_guard<std::mutex> lock(runtime.mu);
  runtime.atomics.push_back(std::make_unique<std::atomic<long long>>(initial));
  return static_cast<long long>(runtime.atomics.size() - 1);
}

[[nodiscard]] bool atomic_fetch(RuntimeContext& runtime, const long long id, const long long delta, long long& previous, std::string& error) {
  std::atomic<long long>* cell = nullptr;
  {
    std::lock_guard<std::mutex> lock(runtime.mu);
    if (id < 0 || static_cast<std::size_t>(id) >= runtime.atomics.size()) {
      return false;
    }
    cell = runtime.atomics[static_cast<std::size_t>(id)].get();
  }
  previous = cell->load();
  while (true) {
    const auto next = numeric::add(previous, delta);
    if (!next) { error = "integer overflow"; return false; }
    if (cell->compare_exchange_weak(previous, *next)) return true;
  }
}

[[nodiscard]] bool atomic_read(RuntimeContext& runtime, const long long id, long long& value) {
  std::atomic<long long>* cell = nullptr;
  {
    std::lock_guard<std::mutex> lock(runtime.mu);
    if (id < 0 || static_cast<std::size_t>(id) >= runtime.atomics.size()) {
      return false;
    }
    cell = runtime.atomics[static_cast<std::size_t>(id)].get();
  }
  value = cell->load();
  return true;
}

[[nodiscard]] long long make_mutex(RuntimeContext& runtime) {
  std::lock_guard<std::mutex> lock(runtime.mu);
  runtime.mutexes.push_back(std::make_unique<RuntimeMutex>());
  return static_cast<long long>(runtime.mutexes.size() - 1);
}

[[nodiscard]] RuntimeMutex* mutex_at(RuntimeContext& runtime, const long long id) {
  std::lock_guard<std::mutex> lock(runtime.mu);
  if (id < 0 || static_cast<std::size_t>(id) >= runtime.mutexes.size()) {
    return nullptr;
  }
  return runtime.mutexes[static_cast<std::size_t>(id)].get();
}

}  // namespace

VirtualMachine::VirtualMachine() : m_runtime(std::make_shared<RuntimeContext>()) {}
VirtualMachine::VirtualMachine(std::shared_ptr<RuntimeContext> runtime)
    : m_runtime(std::move(runtime)), m_runtime_owner(false) {}
void VirtualMachine::release_mutexes() {
  for (const auto handle : m_owned_mutexes) {
    if (auto* cell = mutex_at(*m_runtime, handle)) {
      std::lock_guard<std::mutex> lock(cell->mu);
      if (cell->owner == m_lock_owner) { cell->owner = nullptr; cell->changed.notify_all(); }
    }
  }
  m_runtime->changed.notify_all();
  m_owned_mutexes.clear();
}

void VirtualMachine::cancel_tasks() {
  std::vector<std::shared_ptr<AsyncState>> states;
  { std::lock_guard<std::mutex> lock(m_runtime->mu); states = m_runtime->tasks; }
  for (const auto& state : states) if (state->owner == std::this_thread::get_id() && !state->done && !state->running) {
    state->error = "async task cancelled at execution completion";
    state->machine.reset(); state->done = true;
  }
  m_runtime->changed.notify_all();
}

VirtualMachine::~VirtualMachine() {
  release_mutexes();
  if (m_runtime_owner) { cancel_tasks(); m_runtime->drain(); m_runtime->tasks.clear(); }
  if (m_wait_connection.kind == Value::Kind::Connection) {
    try { m_wait_connection.disconnect(); } catch (const HeapError&) { }
  }
  m_stack.clear(); m_locals.clear(); m_calls.clear(); m_coroutines.clear();
  m_resumer = {}; m_self = Value{}; m_frame_pool.clear();
  m_runtime.reset();
  collect_shared_heap();
}

void VirtualMachine::set_output(std::ostream& out) { m_out = &out; }

void VirtualMachine::set_error_output(std::ostream& err) { m_err = &err; }

void VirtualMachine::isolate() { m_isolated = true; }

void VirtualMachine::load(const BytecodeChunk& chunk) { m_chunk = chunk; m_callback_code = std::make_shared<const BytecodeChunk>(chunk); }

bool VirtualMachine::read_u16(std::uint16_t& index) {
  if (m_ip + 1 >= m_chunk.code.size()) {
    return false;
  }
  const auto lo = static_cast<std::uint16_t>(m_chunk.code[m_ip]);
  ++m_ip;
  const auto hi = static_cast<std::uint16_t>(m_chunk.code[m_ip]);
  ++m_ip;
  index = static_cast<std::uint16_t>(lo | static_cast<std::uint16_t>(hi << 8));
  return true;
}

std::string_view VirtualMachine::error() const { return m_error; }

void VirtualMachine::locate_error(const RegChunk& lowered) {
  m_error_location = {0, 0};
  if (m_error.empty() || lowered.origin.empty()) {
    return;
  }
  const std::size_t index = m_ip == 0 ? 0 : std::min(m_ip - 1, lowered.origin.size() - 1);
  const std::uint16_t pc = lowered.origin[index];
  int best = -1;
  for (const SourceMapEntry& entry : m_chunk.source_map) {
    if (entry.pc <= pc && entry.location.line != 0 && static_cast<int>(entry.pc) >= best) {
      best = entry.pc;
      m_error_location = entry.location;
    }
  }
}

std::vector<Value> VirtualMachine::take_frame(const RegChunk& lowered, const std::size_t function) {
  const std::size_t size = function < lowered.function_frame.size() ? lowered.function_frame[function] : 256;
  std::vector<Value> frame;
  if (!m_frame_pool.empty()) {
    frame = std::move(m_frame_pool.back());
    m_frame_pool.pop_back();
  }
  frame.assign(size, Value::number_of(0));
  return frame;
}

void VirtualMachine::give_frame(std::vector<Value>&& frame) {
  if (m_frame_pool.size() < 64 && frame.capacity() <= 256) {
    frame.clear();
    m_frame_pool.push_back(std::move(frame));
  }
}

void VirtualMachine::set_max_call_depth(const std::size_t depth) { m_max_call_depth = depth == 0 ? 1 : depth; }

void VirtualMachine::collect_garbage() {
  for (const std::unique_ptr<Table>& object : m_heap) {
    if (object != nullptr) {
      object->marked = false;
    }
  }
  const auto mark_values = [](std::vector<Value>& values) {
    for (Value& value : values) vm::mark_value(value);
  };
  const auto mark_frames = [&](std::vector<CallFrame>& frames) {
    for (CallFrame& frame : frames) { mark_values(frame.stack); mark_values(frame.locals); }
  };
  vm::mark_value(m_self);
  mark_values(m_stack);
  mark_values(m_locals);
  mark_frames(m_calls);
  for (Coroutine& routine : m_coroutines) {
    if (!routine.dead) { mark_values(routine.stack); mark_frames(routine.frames); }
  }
  if (m_resumer.live) { mark_values(m_resumer.stack); mark_frames(m_resumer.frames); }
  vm::sweep(m_heap);
  collect_shared_heap();
}

bool VirtualMachine::recover() {
  if (m_traps.empty()) {
    return false;
  }
  const Trap trap = m_traps.back();
  m_traps.pop_back();
  while (m_calls.size() > trap.frames) {
    CallFrame frame = std::move(m_calls.back());
    m_calls.pop_back();
    m_stack = std::move(frame.stack);
    m_locals = std::move(frame.locals);
  }
  m_ip = trap.ip;
  m_recovered = std::string(m_error);
  m_error.clear();
  m_error_location = {0, 0};
  return true;
}

std::size_t VirtualMachine::schedule_task(const Value& task) {
  const auto invalid = static_cast<std::size_t>(-1);
  if (task.task_code() != m_callback_code) {
    m_error = "task belongs to a different executable"; return invalid;
  }
  if (!task.is_task() || !std::isfinite(task.number) || std::trunc(task.number) != task.number ||
      task.number < 0 || task.number >= static_cast<double>(m_chunk.functions.size())) {
    m_error = "invalid async task"; return invalid;
  }
  {
    std::lock_guard<std::mutex> lock(m_runtime->mu);
    for (std::size_t index = 0; index < m_runtime->tasks.size(); ++index)
      if (m_runtime->tasks[index]->task.identity() == task.identity()) return index;
  }
  auto state = std::make_shared<AsyncState>();
  state->task = task; state->owner = std::this_thread::get_id();
  state->machine.reset(new VirtualMachine(m_runtime));
  state->machine->load(m_chunk); state->machine->m_callback_code = m_callback_code;
  state->machine->m_cooperative = true; state->machine->m_isolated = m_isolated || task.w == 1;
  state->machine->m_out = m_out; state->machine->m_err = m_err;
  state->machine->m_max_call_depth = m_max_call_depth;
  state->lowered = lower_registers(m_chunk);
  std::lock_guard<std::mutex> lock(m_runtime->mu);
  for (std::size_t index = 0; index < m_runtime->tasks.size(); ++index)
    if (m_runtime->tasks[index]->task.identity() == task.identity()) return index;
  m_runtime->tasks.push_back(std::move(state));
  return m_runtime->tasks.size() - 1;
}

bool VirtualMachine::pump_tasks() {
  std::vector<std::shared_ptr<AsyncState>> states;
  { std::lock_guard<std::mutex> lock(m_runtime->mu); states = m_runtime->tasks; }
  states.erase(std::remove_if(states.begin(), states.end(), [](const auto& state) {
    return state->owner != std::this_thread::get_id();
  }), states.end());
  std::stable_sort(states.begin(), states.end(), [](const auto& first, const auto& second) {
    return !first->started && second->started;
  });
  bool progressed = false;
  for (const auto& state : states) {
    if (state->owner != std::this_thread::get_id() || state->done || state->running) continue;
    auto& box = *state->machine;
    if (box.m_wait_mutex >= 0) {
      auto* cell = mutex_at(*m_runtime, box.m_wait_mutex);
      if (!cell) continue;
      std::lock_guard<std::mutex> lock(cell->mu);
      if (cell->owner != nullptr) continue;
      box.m_wait_mutex = -1;
    }
    if (box.m_suspended) {
      if (!box.m_wait_parallel.empty()) {
        std::vector<Value> results;
        std::vector<std::shared_ptr<AsyncState>> dependencies;
        bool ready = true;
        for (const auto& task : box.m_wait_parallel) {
          if (task.y == 2) { results.push_back(task.field(0)); continue; }
          std::shared_ptr<AsyncState> dependency;
          { std::lock_guard<std::mutex> lock(m_runtime->mu);
            if (std::isfinite(task.z) && task.z >= 0 && task.z < static_cast<double>(m_runtime->tasks.size())) dependency = m_runtime->tasks[static_cast<std::size_t>(task.z)]; }
          if (!dependency || !dependency->done) { ready = false; break; }
          dependencies.push_back(dependency); results.push_back(dependency->result);
        }
        if (!ready) continue;
        for (const auto& dependency : dependencies) {
          dependency->observed = true;
          if (box.m_error.empty() && !dependency->error.empty()) {
            box.m_error = dependency->error; box.m_error_location = dependency->error_location;
          }
        }
        box.m_wait_parallel.clear();
        if (!box.m_error.empty()) {
          if (!box.recover()) {
            state->error = box.m_error; state->error_location = box.m_error_location;
            state->machine.reset(); state->lowered = {}; state->done = true; progressed = true; m_runtime->changed.notify_all(); continue;
          }
        } else box.m_stack[box.m_wait_slot] = Value::struct_of(std::move(results));
      } else if (box.m_wait_event.is_task()) {
        if (box.m_wait_event.field(0).integer == 0) continue;
        box.m_stack[box.m_wait_slot] = box.m_wait_event.field(1);
        box.m_wait_event = {}; box.m_wait_connection = {};
      } else {
        std::shared_ptr<AsyncState> dependency;
        { std::lock_guard<std::mutex> lock(m_runtime->mu);
          if (box.m_wait_index < m_runtime->tasks.size()) dependency = m_runtime->tasks[box.m_wait_index]; }
        if (!dependency || !dependency->done) continue;
        dependency->observed = true;
        if (!dependency->error.empty()) {
          box.m_error = dependency->error; box.m_error_location = dependency->error_location;
          if (!box.recover()) {
            state->error = box.m_error; state->error_location = box.m_error_location;
            state->machine.reset(); state->lowered = {}; state->done = true; progressed = true; m_runtime->changed.notify_all(); continue;
          }
        } else box.m_stack[box.m_wait_slot] = dependency->result;
      }
      box.m_wait_index = static_cast<std::size_t>(-1); box.m_suspended = false;
    }
    state->running = true; box.m_yielded = false; box.m_budget = 1024;
    bool ok;
    if (!state->started) {
      state->started = true;
      ok = box.run_function(static_cast<std::uint16_t>(state->task.number), state->task.snapshot_fields(), state->result);
    } else {
      ok = box.execute_registers(state->lowered);
      if (ok && !box.m_suspended && !box.m_yielded) state->result = box.m_stack[0];
    }
    state->running = false; progressed = true;
    if (!ok || (!box.m_suspended && !box.m_yielded)) {
      if (!ok) {
        if (box.m_error_location.line == 0) box.locate_error(state->lowered);
        state->error = box.m_error; state->error_location = box.m_error_location;
      }
      state->machine.reset(); state->lowered = {}; state->done = true; m_runtime->changed.notify_all();
    }
  }
  return progressed;
}

bool VirtualMachine::await_task(const std::size_t index, Value& out) {
  std::shared_ptr<AsyncState> state;
  { std::lock_guard<std::mutex> lock(m_runtime->mu);
    if (index < m_runtime->tasks.size()) state = m_runtime->tasks[index]; }
  if (!state) { m_error = "invalid async task"; return false; }
  while (!state->done) {
    if (pump_tasks()) continue;
    bool producer = false;
    std::unique_lock<std::mutex> lock(m_runtime->mu);
    producer = state->owner != std::this_thread::get_id() && m_runtime->schedulers.contains(state->owner);
    for (const auto& other : m_runtime->tasks) {
      if (other->owner != std::this_thread::get_id() && !other->done && m_runtime->schedulers.contains(other->owner)) {
        producer = true; break;
      }
    }
    for (const auto& worker : m_runtime->threads) {
      std::lock_guard<std::mutex> worker_lock(worker->mu);
      if (!worker->done && worker->worker.get_id() != std::this_thread::get_id()) { producer = true; break; }
    }
    if (!producer) {
      m_error = "async tasks are waiting with no event producer"; return false;
    }
    m_runtime->changed.wait_for(lock, std::chrono::milliseconds(20));
  }
  state->observed = true;
  if (!state->error.empty()) { m_error = state->error; m_error_location = state->error_location; return false; }
  out = state->result; return true;
}

bool VirtualMachine::complete_tasks() {
  std::size_t index = 0;
  while (true) {
    std::shared_ptr<AsyncState> state;
    { std::lock_guard<std::mutex> lock(m_runtime->mu);
      if (index == m_runtime->tasks.size()) break;
      state = m_runtime->tasks[index++]; }
    if (state->owner != std::this_thread::get_id() || state->observed) continue;
    Value ignored;
    if (!await_task(index - 1, ignored)) return false;
  }
  return true;
}

bool VirtualMachine::execute_registers(const RegChunk& lowered) {
  RuntimeContext& runtime = *m_runtime;
  const auto reg = [&](const std::uint8_t index) -> Value* {
    if (index >= m_stack.size()) {
      m_error = "runtime error";
      return nullptr;
    }
    return &m_stack[index];
  };
  const auto run_now = [&](const std::uint16_t index, const std::vector<Value>& args, Value& out) {
    static thread_local std::size_t callback_depth = 0;
    if (callback_depth >= 64) { m_error = "synchronous callback nesting limit exceeded"; return false; }
    struct DepthGuard { std::size_t& depth; explicit DepthGuard(std::size_t& d) : depth(d) { ++depth; } ~DepthGuard() { --depth; } } guard(callback_depth);
    VirtualMachine box(m_runtime);
    box.m_lock_owner = m_lock_owner;
    box.m_isolated = m_isolated;
    box.load(m_chunk);
    box.m_callback_code = m_callback_code;
    if (m_out != nullptr) {
      box.set_output(*m_out);
    }
    if (m_err != nullptr) box.set_error_output(*m_err);
    if (!box.run_function(index, args, out)) {
      m_error = std::string(box.error().empty() ? "runtime error" : box.error());
      m_error_location = box.error_location();
      return false;
    }
    return true;
  };
  const auto write_cell = [&](const Value& cell, const Value& value, bool store = true) {
    Value target = cell; if (store) target.set_field(0, value);
    const auto subscriptions = target.connections();
    for (auto connection : subscriptions) {
      const auto callback = connection.take_callback();
      if (callback.is_null()) continue;
      if (!callback.is_closure() || callback.closure_code() != m_callback_code) { m_error = "invalid observable callback"; return false; }
      auto arguments = callback.snapshot_fields();
      if (callback.y == 1) arguments.push_back(value);
      else arguments.insert(arguments.begin(), value);
      Value result;
      if (!run_now(static_cast<std::uint16_t>(callback.integer), arguments, result)) return false;
    }
    return true;
  };
  const auto launch = [&](const std::uint16_t index, std::vector<Value> args, const bool actor = false) {
    const BytecodeChunk chunk = m_chunk;
    const auto callback_code = m_callback_code;
    std::ostream* const out = m_out;
    std::ostream* const err = m_err;
    const bool isolated = m_isolated || actor;
    const auto state = std::make_shared<ThreadSlot>();
    state->worker = std::thread([state, chunk, callback_code, args = std::move(args), index, out, err, isolated, context = m_runtime]() {
      VirtualMachine worker(context);
      worker.m_complete_on_return = true;
      worker.m_isolated = isolated;
      worker.load(chunk);
      worker.m_callback_code = callback_code;
      if (out != nullptr) {
        worker.set_output(*out);
      }
      if (err != nullptr) worker.set_error_output(*err);
      Value result = Value::number_of(0);
      std::string error;
      if (!worker.run_function(index, args, result)) {
        error = std::string(worker.error().empty() ? "runtime error" : worker.error());
      }
      {
        std::lock_guard<std::mutex> lock(state->mu);
        state->result = std::move(result);
        state->error = std::move(error);
        state->error_location = worker.error_location();
        state->done = true;
      }
      state->cv.notify_one();
      context->changed.notify_all();
    });
    std::lock_guard<std::mutex> lock(runtime.mu);
    runtime.threads.push_back(state);
    return static_cast<long long>(runtime.threads.size() - 1);
  };
  const auto finish = [&](const long long handle, Value& out) {
    std::shared_ptr<ThreadSlot> state;
    {
      std::lock_guard<std::mutex> lock(runtime.mu);
      if (handle < 0 || static_cast<std::size_t>(handle) >= runtime.threads.size()) {
        m_error = "runtime error";
        return false;
      }
      state = runtime.threads[static_cast<std::size_t>(handle)];
    }
    std::unique_lock<std::mutex> lock(state->mu);
    while (!state->done) {
      lock.unlock(); (void)pump_tasks(); lock.lock();
      if (!state->done) state->cv.wait_for(lock, std::chrono::milliseconds(20));
    }
    state->observed = true;
    if (!state->error.empty()) {
      m_error = state->error;
      m_error_location = state->error_location;
      return false;
    }
    out = state->result;
    return true;
  };

  std::size_t ticks = 0;
  while (m_ip < lowered.code.size()) {
    if (m_cooperative) {
      if (m_budget == 0) { m_yielded = true; return true; }
      --m_budget;
    } else if (++ticks >= 256) { ticks = 0; (void)pump_tasks(); }
    const std::uint32_t word = lowered.code[m_ip];
    ++m_ip;
    const auto op = static_cast<RegOp>(reg_op(word));
    const std::uint8_t a = reg_a(word);
    const std::uint8_t b = reg_b(word);
    const std::uint8_t c = reg_c(word);
    const std::uint16_t d = reg_d(word);
    Value* dest = reg(a);
    Value* left = reg(b);
    Value* right = reg(c);
    if (dest == nullptr || left == nullptr || right == nullptr) {
      return false;
    }

    try {
    switch (op) {
      case RegOp::Freeze:
        dest->freeze();
        break;
      case RegOp::Nop:
        break;
      case RegOp::Halt:
        if (!m_deferred.empty()) {
          const std::uint16_t function_index = m_deferred.front();
          m_deferred.erase(m_deferred.begin());
          if (function_index >= m_chunk.functions.size() || function_index >= lowered.function_entry.size() ||
              m_chunk.functions[function_index].arity != 0) {
            m_error = "runtime error";
            return false;
          }
          CallFrame frame;
          frame.ip = m_ip - 1;
          frame.stack = m_stack;
          m_calls.push_back(std::move(frame));
          m_stack.assign(256, Value::number_of(0));
          m_ip = lowered.function_entry[function_index];
          break;
        }
        return true;
      case RegOp::LoadK:
        if (d >= m_chunk.constants.size()) {
          m_error = "runtime error";
          return false;
        }
        *dest = m_chunk.constants[d];
        break;
      case RegOp::Move:
        *dest = *left;
        break;
      case RegOp::Clear:
        if (dest->is_string()) {
          *dest = Value::string_of("");
        } else if (dest->is_struct() || dest->is_task()) {
          *dest = Value::struct_of({});
        } else if (dest->is_vector()) {
          *dest = Value::vector_of(0, 0, 0, 0);
        } else {
          dest->set_number(0);
        }
        break;
      case RegOp::Add:
      case RegOp::IntAdd:
      case RegOp::Sub:
      case RegOp::Mul:
      case RegOp::Div:
      case RegOp::Mod: {
        if (op == RegOp::Add && left->is_vector() && right->is_vector()) {
          *dest = Value::vector_of(left->number + right->number, left->y + right->y, left->z + right->z, left->w + right->w,
                                   std::max(left->dims, right->dims));
          break;
        }
        if (op == RegOp::Sub && left->is_vector() && right->is_vector()) {
          *dest = Value::vector_of(left->number - right->number, left->y - right->y, left->z - right->z, left->w - right->w,
                                   std::max(left->dims, right->dims));
          break;
        }
        if ((op == RegOp::Mul || op == RegOp::Div) && left->is_vector() && right->is_number()) {
          if (op == RegOp::Div && right->number == 0) {
            m_error = "division by zero";
            if (recover()) {
              continue;
            }
            return false;
          }
          const double factor = op == RegOp::Mul ? right->number : 1.0 / right->number;
          *dest = Value::vector_of(left->number * factor, left->y * factor, left->z * factor, left->w * factor, left->dims);
          break;
        }
        if (op == RegOp::Mul && left->is_number() && right->is_vector()) {
          *dest = Value::vector_of(right->number * left->number, right->y * left->number, right->z * left->number,
                                   right->w * left->number, right->dims);
          break;
        }
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        if ((op == RegOp::Div || op == RegOp::Mod) && right->number == 0) {
          m_error = "division by zero";
          if (recover()) {
            continue;
          }
          return false;
        }
        if (left->is_integer() && right->is_integer()) {
          const auto lhs = left->integer, rhs = right->integer;
          const auto result = op == RegOp::Add || op == RegOp::IntAdd ? numeric::add(lhs, rhs) :
                              op == RegOp::Sub ? numeric::subtract(lhs, rhs) :
                              op == RegOp::Mul ? numeric::multiply(lhs, rhs) :
                              op == RegOp::Div ? numeric::divide(lhs, rhs) : numeric::remainder(lhs, rhs);
          if (!result) {
            m_error = "integer overflow";
            if (recover()) continue;
            return false;
          }
          dest->set_integer(*result);
          break;
        }
        double result = 0;
        if (op == RegOp::Add || op == RegOp::IntAdd) {
          result = left->number + right->number;
        } else if (op == RegOp::Sub) {
          result = left->number - right->number;
        } else if (op == RegOp::Mul) {
          result = left->number * right->number;
        } else if (op == RegOp::Div) {
          result = left->number / right->number;
        } else {
          result = std::fmod(left->number, right->number);
        }
        dest->set_number(result);
        break;
      }
      case RegOp::BitAnd:
      case RegOp::BitOr:
      case RegOp::BitXor:
      case RegOp::Shl:
      case RegOp::Shr: {
        if (left == nullptr || right == nullptr || !left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        const auto lhs_value = left->is_integer() ? std::optional<std::int64_t>{left->integer} : numeric::from_float(left->number);
        const auto rhs_value = right->is_integer() ? std::optional<std::int64_t>{right->integer} : numeric::from_float(right->number);
        if (!lhs_value || !rhs_value) { m_error = "integer out of range"; return false; }
        const auto lhs = *lhs_value, rhs = *rhs_value;
        long long result = 0;
        if (op == RegOp::BitAnd) {
          result = lhs & rhs;
        } else if (op == RegOp::BitOr) {
          result = lhs | rhs;
        } else if (op == RegOp::BitXor) {
          result = lhs ^ rhs;
        } else if (op == RegOp::Shl) {
          result = std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(lhs) << (rhs & 63));
        } else {
          result = lhs >> (rhs & 63);
        }
        dest->set_integer(result);
        break;
      }
      case RegOp::BitNot: {
        if (left == nullptr || !left->is_number()) {
          m_error = "type error";
          return false;
        }
        const auto value = left->is_integer() ? std::optional<std::int64_t>{left->integer} : numeric::from_float(left->number);
        if (!value) { m_error = "integer out of range"; return false; }
        dest->set_integer(~*value);
        break;
      }
      case RegOp::Range: {
        const auto lo = integral_value(*left), hi = integral_value(*right);
        if (!lo || !hi) { m_error = "range bounds must be integers"; if (recover()) continue; return false; }
        const auto count = *hi > *lo ? numeric::subtract(*hi, *lo) : std::optional<std::int64_t>{0};
        std::vector<Value> values;
        if (!count || static_cast<std::uint64_t>(*count) > values.max_size()) {
          m_error = "range size out of range"; if (recover()) continue; return false;
        }
        try { values.reserve(static_cast<std::size_t>(*count)); }
        catch (const std::bad_alloc&) { m_error = "range allocation failed"; if (recover()) continue; return false; }
        catch (const std::length_error&) { m_error = "range size out of range"; if (recover()) continue; return false; }
        for (auto cursor = *lo; cursor < *hi; ++cursor) values.push_back(Value::integer_of(cursor));
        *dest = Value::struct_of(std::move(values));
        break;
      }
      case RegOp::Slice: {
        if (static_cast<std::size_t>(a) + 3 > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        const Value object = m_stack[a];
        const Value start = m_stack[static_cast<std::size_t>(a) + 1];
        const Value bound = m_stack[static_cast<std::size_t>(a) + 2];
        if (!start.is_number() || !bound.is_number()) {
          m_error = "type error";
          return false;
        }
        const auto lo = integral_value(start), hi = integral_value(bound);
        if (!lo || !hi) { m_error = "slice bounds must be integers"; if (recover()) continue; return false; }
        if (object.is_string()) {
          std::size_t from = object.text.size(), to = object.text.size(), character = 0, offset = 0;
          const auto first = *lo < 0 ? std::uint64_t{0} : static_cast<std::uint64_t>(*lo);
          const auto last = *hi < 0 ? std::uint64_t{0} : static_cast<std::uint64_t>(*hi);
          while (offset < object.text.size()) {
            if (character == first) from = offset;
            if (character == last) { to = offset; break; }
            offset += utf8_width(object.text, offset); ++character;
          }
          *dest = Value::string_of(from >= to ? std::string{} : object.text.substr(from, to - from));
          break;
        }
        if (object.is_struct() && object.number == 0) {
          const auto fields = object.snapshot_fields();
          const auto clamp = [&](const auto value) { return value < 0 ? std::size_t{0} : static_cast<std::size_t>(std::min(static_cast<std::uint64_t>(value), static_cast<std::uint64_t>(fields.size()))); };
          const auto from = clamp(*lo), to = clamp(*hi);
          std::vector<Value> values;
          if (from < to) values.assign(fields.begin() + from, fields.begin() + to);
          *dest = Value::struct_of(std::move(values));
          break;
        }
        m_error = "slicing expects an array, list or string";
        if (recover()) continue;
        return false;
      }
      case RegOp::PushError:
        *dest = Value::string_of(m_recovered);
        break;
      case RegOp::Concat: {
        // Any value joins as text: "pos " .: Vector3(1, 2, 3) is "pos (1, 2, 3)".
        *dest = Value::string_of(format_value(*left) + format_value(*right));
        break;
      }
      case RegOp::ConvertNumber:
      case RegOp::ConvertFloat:
      case RegOp::ConvertOptionalFloat: {
        if (op == RegOp::ConvertOptionalFloat && left->is_null()) { *dest = Value::null_of(); break; }
        if (!left->is_number()) { m_error = "type error"; if (recover()) continue; return false; }
        if (op == RegOp::ConvertFloat || op == RegOp::ConvertOptionalFloat) {
          const double result = left->is_integer() ? static_cast<double>(left->integer) : left->number;
          dest->set_number(result);
        } else if (left->is_integer()) {
          const auto result = left->integer; dest->set_integer(result);
        } else {
          const auto result = numeric::from_float(left->number);
          if (!result) { m_error = "numeric cast out of range"; if (recover()) continue; return false; }
          dest->set_integer(*result);
        }
        break;
      }
      case RegOp::Not:
        dest->set_boolean(!is_truthy(*left));
        break;
      case RegOp::Eq:
      case RegOp::NotEq:
        if (left->is_string() && right->is_string()) {
          const bool same = left->text == right->text;
          dest->set_boolean(op == RegOp::Eq ? same : !same);
          break;
        }
        if (!left->is_number() || !right->is_number()) {
          // structs, lists and vectors compare by value, field by field
          const bool same = values_equal(*left, *right);
          dest->set_boolean(op == RegOp::Eq ? same : !same);
          break;
        }
        [[fallthrough]];
      case RegOp::Less:
      case RegOp::LessEq:
      case RegOp::Greater:
      case RegOp::GreaterEq: {
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        bool result = false;
        if (left->is_integer() || right->is_integer()) {
          std::optional<int> order;
          if (left->is_integer() && right->is_integer()) {
            order = left->integer < right->integer ? -1 : left->integer > right->integer ? 1 : 0;
          } else if (left->is_integer()) {
            order = numeric::compare(left->integer, right->number);
          } else {
            order = numeric::compare(right->integer, left->number);
            if (order) *order = -*order;
          }
          result = op == RegOp::NotEq ? !order || *order != 0 : order &&
              (op == RegOp::Eq ? *order == 0 : op == RegOp::Less ? *order < 0 :
               op == RegOp::LessEq ? *order <= 0 : op == RegOp::Greater ? *order > 0 : *order >= 0);
          dest->set_boolean(result);
          break;
        }
        if (op == RegOp::Eq) {
          result = left->number == right->number;
        } else if (op == RegOp::NotEq) {
          result = left->number != right->number;
        } else if (op == RegOp::Less) {
          result = left->number < right->number;
        } else if (op == RegOp::LessEq) {
          result = left->number <= right->number;
        } else if (op == RegOp::Greater) {
          result = left->number > right->number;
        } else {
          result = left->number >= right->number;
        }
        dest->set_boolean(result);
        break;
      }
      case RegOp::Jump:
        m_ip = d;
        break;
      case RegOp::JumpIfFalse:
        if (!is_truthy(*dest)) {
          m_ip = d;
        }
        break;
      case RegOp::Post:
        if (m_out != nullptr) {
          std::lock_guard<std::mutex> output_lock(runtime.output_mu);
          (*m_out) << format_value(*dest) << '\n';
        }
        break;
      case RegOp::Warn:
        if (m_err != nullptr) {
          std::lock_guard<std::mutex> output_lock(runtime.output_mu);
          (*m_err) << "warning: " << format_value(*dest) << '\n';
        }
        break;
      case RegOp::Report:
        // report(x) stops the program with x as the error message; try/catch and pcall catch it.
        m_error = format_value(*dest);
        if (recover()) {
          continue;
        }
        return false;
      case RegOp::Tag:
        if (!dest->is_struct()) {
          m_error = "type error";
          return false;
        }
        dest->number = d;
        break;
      case RegOp::VCall: {
        if (!m_stack[a].is_struct() || m_stack[a].number < 1) {
          m_error = "type error";
          return false;
        }
        const auto type_index = static_cast<std::size_t>(static_cast<std::uint16_t>(m_stack[a].number) - 1);
        if (type_index >= m_chunk.vtables.size() || d >= m_chunk.vtables[type_index].size()) {
          m_error = "type error";
          return false;
        }
        const std::uint16_t callee = m_chunk.vtables[type_index][d];
        if (callee >= m_chunk.functions.size() || callee >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[callee];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        if (function.is_async) {
          std::vector<Value> arguments(function.arity);
          for (std::uint16_t index = 0; index < function.arity; ++index) arguments[index] = m_stack[static_cast<std::size_t>(a) + index];
          Value task = Value::task_fields(std::move(arguments), m_callback_code); task.number = callee; task.w = m_isolated ? 1 : 0;
          *dest = std::move(task); break;
        }
        if (m_calls.size() >= m_max_call_depth) {
          m_error = "stack overflow";
          return false;
        }
        std::vector<Value> frame = take_frame(lowered, callee);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          frame[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        m_calls.push_back(CallFrame{m_ip, std::move(m_stack), {}, a, function.arity});
        m_stack = std::move(frame);
        m_ip = lowered.function_entry[callee];
        break;
      }
      case RegOp::Globals: {
        std::lock_guard<std::mutex> lock(runtime.mu);
        if (!runtime.globals.empty()) { m_error = "module storage already initialized"; return false; }
        runtime.globals.reserve(d);
        for (std::uint16_t i = 0; i < d; ++i) runtime.globals.push_back(Value::cell_of(Value::null_of()));
        runtime.global_ready.assign(d, false); runtime.global_const.assign(d, false); break;
      }
      case RegOp::LoadGlobal:
      case RegOp::RawGlobal:
      case RegOp::StoreGlobal:
      case RegOp::InitGlobal:
      case RegOp::InitConstGlobal: {
        if ((op == RegOp::StoreGlobal || op == RegOp::InitGlobal || op == RegOp::InitConstGlobal) &&
            !transferable_values({*dest})) { m_error = "module state cannot retain VM-local table values"; if (recover()) continue; return false; }
        Value cell;
        {
          std::lock_guard<std::mutex> lock(runtime.mu);
          if (d >= runtime.globals.size()) { m_error = "module binding out of range"; return false; }
          const bool initialize = op == RegOp::InitGlobal || op == RegOp::InitConstGlobal;
          if (!initialize && !runtime.global_ready[d]) m_error = "module binding used before initialization";
          else if (initialize && runtime.global_ready[d]) m_error = "module binding already initialized";
          else if (op == RegOp::StoreGlobal && runtime.global_const[d]) m_error = "cannot assign to immutable module binding";
          else {
            cell = runtime.globals[d];
            if (initialize) { cell.set_field(0, *dest); runtime.global_ready[d] = true; runtime.global_const[d] = op == RegOp::InitConstGlobal; }
          }
        }
        if (!m_error.empty()) { if (recover()) continue; return false; }
        if (op == RegOp::LoadGlobal) *dest = cell.field(0);
        else if (op == RegOp::RawGlobal) *dest = std::move(cell);
        else if (!write_cell(cell, *dest, op == RegOp::StoreGlobal)) { if (recover()) continue; return false; }
        break;
      }
      case RegOp::BoxLocal:
        *dest = Value::cell_of(dest->is_cell() ? dest->field(0) : std::move(*dest)); break;
      case RegOp::LoadCell:
        if (!left->is_cell()) { m_error = "invalid capture cell"; return false; }
        *dest = left->field(0); break;
      case RegOp::StoreCell:
        if (!dest->is_cell()) { m_error = "invalid capture cell"; return false; }
        if (!write_cell(*dest, *left)) { if (recover()) continue; return false; }
        break;
      case RegOp::Observe:
        if (!left->is_cell() || !right->is_closure() || right->closure_code() != m_callback_code) {
          m_error = "invalid observable callback"; if (recover()) continue; return false;
        }
        if (static_cast<std::size_t>(right->integer) >= m_chunk.functions.size() || m_chunk.functions[right->integer].is_async)
          throw HeapError("observable requires a synchronous callback");
        *dest = left->subscribe(*right, false, false); break;
      case RegOp::ClearCell:
        if (!dest->is_cell()) { m_error = "invalid capture cell"; return false; }
        dest->set_field(0, Value::null_of()); break;
      case RegOp::Event: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) throw HeapError("invalid event arguments");
        std::vector<Value> args;
        for (std::uint8_t index = 0; index < b; ++index) args.push_back(m_stack[static_cast<std::size_t>(a) + index]);
        if (c == 0 && args.empty()) { *dest = Value::signal_of(); break; }
        if (args.empty()) throw HeapError("invalid event arguments");
        if (c == 1 || c == 5 || c == 9) {
          if (args.size() != 2 || args[0].kind != Value::Kind::Signal || !args[1].is_closure() || args[1].closure_code() != m_callback_code)
            throw HeapError("invalid event callback");
          const auto function = static_cast<std::uint16_t>(args[1].integer);
          if (function >= m_chunk.functions.size() || m_chunk.functions[function].is_async) throw HeapError("event requires a synchronous callback");
          *dest = args[0].subscribe(args[1], c == 5, c == 9); break;
        }
        if (c == 3 || c == 4) {
          if (args.size() != 1 || args[0].kind != Value::Kind::Connection) throw HeapError("invalid connection");
          if (c == 3) { args[0].disconnect(); *dest = Value::null_of(); collect_garbage(); }
          else dest->set_boolean(args[0].connected());
          break;
        }
        if (c == 10) {
          if (args.size() != 1 || args[0].kind != Value::Kind::Signal) throw HeapError("invalid signal");
          args[0].clear_managed(); *dest = Value::null_of(); collect_garbage(); break;
        }
        if (c == 11) {
          if (!m_cooperative || args.size() != 1 || args[0].kind != Value::Kind::Signal)
            throw HeapError("event Wait requires a cooperative async task");
          m_wait_event = Value::task_fields({Value::integer_of(0), Value::null_of()});
          m_wait_event.y = 3;
          m_wait_connection = args[0].subscribe(m_wait_event, true, false);
          m_wait_slot = a; m_suspended = true; return true;
        }
        if (c != 2 || args[0].kind != Value::Kind::Signal) throw HeapError("invalid event operation");
        const auto subscriptions = args[0].connections();
        bool failed = false;
        for (auto connection : subscriptions) {
          const auto callback = connection.take_callback(); if (callback.is_null()) continue;
          if (callback.is_task() && callback.y == 3) {
            Value waiter = callback;
            Value payload = args.size() == 1 ? Value::null_of() : args.size() == 2 ? args[1]
                : Value::struct_of(std::vector<Value>(args.begin() + 1, args.end()));
            waiter.set_field(1, payload); waiter.set_field(0, Value::integer_of(1));
            runtime.changed.notify_all(); continue;
          }
          if (!callback.is_closure() || callback.closure_code() != m_callback_code) throw HeapError("invalid event callback");
          auto captures = callback.snapshot_fields(); std::vector<Value> arguments;
          if (callback.y == 1) { if (captures.size() != 1) throw HeapError("invalid bound callback"); arguments.push_back(captures[0]); captures.clear(); }
          arguments.insert(arguments.end(), args.begin() + 1, args.end());
          arguments.insert(arguments.end(), captures.begin(), captures.end());
          Value result;
          if (!run_now(static_cast<std::uint16_t>(callback.integer), arguments, result)) { failed = true; break; }
        }
        if (failed) { if (recover()) continue; return false; }
        *dest = Value::null_of(); break;
      }
      case RegOp::MakeClosure: {
        auto cells = dest->snapshot_fields();
        for (const auto& cell : cells) if (!cell.is_cell()) { m_error = "invalid capture cell"; return false; }
        *dest = Value::closure_of(d, std::move(cells), m_callback_code); break;
      }
      case RegOp::MakeBoundMethod: {
        const Value receiver = *dest;
        if (!receiver.is_struct() || receiver.number < 1) { m_error = "invalid method receiver"; if (recover()) continue; return false; }
        const auto type = static_cast<std::size_t>(static_cast<std::uint16_t>(receiver.number) - 1);
        if (type >= m_chunk.vtables.size() || d >= m_chunk.vtables[type].size()) { m_error = "invalid method slot"; return false; }
        const auto callee = m_chunk.vtables[type][d];
        if (callee >= m_chunk.functions.size() || m_chunk.functions[callee].arity == 0) { m_error = "invalid method"; return false; }
        *dest = Value::closure_of(callee, {receiver}, m_callback_code); dest->y = 1; break;
      }
      case RegOp::Invoke:
      case RegOp::InvokeThread: {
        const Value callback = *dest;
        if (!callback.is_closure() || callback.closure_code() != m_callback_code) {
          m_error = "callback belongs to a different executable"; return false;
        }
        const auto callee = static_cast<std::uint16_t>(callback.integer);
        if (callee >= m_chunk.functions.size() || callee >= lowered.function_entry.size() ||
            static_cast<std::size_t>(a) + d + 1 > m_stack.size()) { m_error = "invalid callback"; return false; }
        auto cells = callback.snapshot_fields();
        if (static_cast<std::size_t>(d) + cells.size() != m_chunk.functions[callee].arity) {
          m_error = "callback argument count mismatch"; return false;
        }
        std::vector<Value> arguments;
        arguments.reserve(d + cells.size());
        if (callback.y == 1) {
          if (cells.size() != 1 || !cells[0].is_struct()) { m_error = "invalid bound method"; return false; }
          arguments.push_back(std::move(cells[0])); cells.clear();
        }
        for (std::uint16_t i = 0; i < d; ++i) arguments.push_back(m_stack[static_cast<std::size_t>(a) + 1 + i]);
        for (auto& cell : cells) arguments.push_back(std::move(cell));
        if (m_chunk.functions[callee].is_async) {
          if (op == RegOp::InvokeThread) { m_error = "use spawn for an async callback"; if (recover()) continue; return false; }
          Value task = Value::task_fields(std::move(arguments), m_callback_code);
          task.number = callee;
          task.w = m_isolated ? 1 : 0;
          *dest = std::move(task);
          break;
        }
        if (op == RegOp::InvokeThread) { dest->set_integer(launch(callee, std::move(arguments))); break; }
        if (m_calls.size() >= m_max_call_depth) { m_error = "stack overflow"; return false; }
        auto frame = take_frame(lowered, callee);
        for (std::size_t i = 0; i < arguments.size(); ++i) frame[i] = std::move(arguments[i]);
        m_calls.push_back(CallFrame{m_ip, std::move(m_stack), {}, a, m_chunk.functions[callee].arity});
        m_stack = std::move(frame); m_ip = lowered.function_entry[callee]; break;
      }
      case RegOp::Call: {
        if (m_calls.size() >= m_max_call_depth) {
          m_error = "stack overflow";
          return false;
        }
        if (d >= m_chunk.functions.size() || d >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        // The callee gets a frame sized to what it uses (not 256 registers), recycled from a pool.
        std::vector<Value> frame = take_frame(lowered, d);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          frame[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        m_calls.push_back(CallFrame{m_ip, std::move(m_stack), {}, a, function.arity});
        m_stack = std::move(frame);
        m_ip = lowered.function_entry[d];
        break;
      }
      case RegOp::Schedule: {
        if (d >= m_chunk.functions.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> args(function.arity);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          args[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        Value task;
        task.kind = Value::Kind::Task;
        task.number = d;
        task.y = 0;
        task = Value::task_fields(std::move(args), m_callback_code);
        task.number = d;
        task.w = m_isolated ? 1 : 0;
        *dest = std::move(task);
        break;
      }
      case RegOp::CoCreate: {
        Coroutine created;
        created.function = d;
        m_coroutines.push_back(std::move(created));
        dest->set_number(static_cast<double>(m_coroutines.size()));
        break;
      }
      case RegOp::Defer:
        m_deferred.push_back(d);
        dest->set_number(0);
        break;
      case RegOp::CoResume: {
        if (m_resumer.live || !left->is_number() || left->number < 1) {
          m_error = "type error";
          return false;
        }
        const auto index = static_cast<std::size_t>(left->number) - 1;
        if (index >= m_coroutines.size() || m_coroutines[index].dead) {
          m_error = "type error";
          return false;
        }
        Coroutine& routine = m_coroutines[index];
        if (routine.function >= lowered.function_entry.size() || m_chunk.functions[routine.function].arity != 0) {
          m_error = "runtime error";
          return false;
        }
        m_resumer.live = true;
        m_resumer.ip = m_ip;
        m_resumer.stack = m_stack;
        m_resumer.frames = m_calls;
        m_resumer.base = a;
        m_active = static_cast<int>(index);
        if (!routine.started) {
          routine.started = true;
          m_calls.clear();
          CallFrame root;
          root.co_root = true;
          m_calls.push_back(std::move(root));
          m_stack.assign(256, Value::number_of(0));
          m_ip = lowered.function_entry[routine.function];
        } else {
          m_calls = routine.frames;
          m_stack = routine.stack;
          m_ip = routine.ip;
        }
        break;
      }
      case RegOp::CoYield: {
        if (m_active < 0 || !m_resumer.live || static_cast<std::size_t>(m_active) >= m_coroutines.size()) {
          m_error = "type error";
          return false;
        }
        const Value yielded = *left;
        dest->set_number(0);
        Coroutine& routine = m_coroutines[static_cast<std::size_t>(m_active)];
        routine.started = true;
        routine.ip = m_ip;
        routine.stack = m_stack;
        routine.frames = m_calls;
        m_ip = m_resumer.ip;
        m_stack = std::move(m_resumer.stack);
        m_calls = std::move(m_resumer.frames);
        if (m_resumer.base >= m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        m_stack[m_resumer.base] = yielded;
        m_resumer.live = false;
        m_active = -1;
        break;
      }
      case RegOp::Actor: {
        if (d >= m_chunk.functions.size() || m_chunk.functions[d].arity != 1) {
          m_error = "runtime error";
          return false;
        }
        Value result = Value::number_of(0);
        const auto handle = launch(d, {*dest}, true);
        if (!finish(handle, result)) return false;
        *dest = std::move(result);
        break;
      }
      case RegOp::Return: {
        Value result = *dest;
        if (!m_calls.empty() && m_calls.back().co_root) {
          m_calls.pop_back();
          if (m_active >= 0 && static_cast<std::size_t>(m_active) < m_coroutines.size()) {
            m_coroutines[static_cast<std::size_t>(m_active)].dead = true;
          }
          if (!m_resumer.live) {
            m_error = "runtime error";
            return false;
          }
          m_ip = m_resumer.ip;
          m_stack = std::move(m_resumer.stack);
          m_calls = std::move(m_resumer.frames);
          if (m_resumer.base >= m_stack.size()) {
            m_error = "runtime error";
            return false;
          }
          m_stack[m_resumer.base] = std::move(result);
          m_resumer.live = false;
          m_active = -1;
          break;
        }
        if (m_calls.empty()) {
          return true;
        }
        CallFrame frame = std::move(m_calls.back());
        m_calls.pop_back();
        m_ip = frame.ip;
        give_frame(std::move(m_stack));
        m_stack = std::move(frame.stack);
        if (frame.base >= m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        m_stack[frame.base] = std::move(result);
        break;
      }
      case RegOp::MakeVector: {
        if (static_cast<std::size_t>(a) + c > m_stack.size() || c < 2 || c > 4) {
          m_error = "type error";
          return false;
        }
        double parts[4] = {0, 0, 0, 0};
        for (std::uint8_t index = 0; index < c; ++index) {
          const Value& part = m_stack[static_cast<std::size_t>(a) + index];
          if (!part.is_number()) {
            m_error = "type error";
            return false;
          }
          parts[index] = part.number;
        }
        *dest = Value::vector_of(parts[0], parts[1], parts[2], parts[3], c);
        break;
      }
      case RegOp::MakeBuffer:
        if (!left->is_number() || left->number < 0) {
          m_error = "type error";
          return false;
        }
        *dest = Value::buffer_of(left->number);
        break;
      case RegOp::BufferWrite:
        if (static_cast<std::size_t>(a) + 2 >= m_stack.size() || !m_stack[a].is_buffer() || !m_stack[a + 1].is_number() ||
            !m_stack[static_cast<std::size_t>(a) + 2].is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(0);
        break;
      case RegOp::BufferSize:
        if (!left->is_buffer()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(left->number);
        break;
      case RegOp::GetField: {
        const Value object = *left;
        if (object.is_struct()) {
          if (c >= object.field_count()) {
            m_error = "type error";
            return false;
          }
          *dest = object.field(c);
          break;
        }
        if (!object.is_vector() || c > 3) {
          m_error = "type error";
          return false;
        }
        double component = object.number;
        if (c == 1) {
          component = object.y;
        } else if (c == 2) {
          component = object.z;
        } else if (c == 3) {
          component = object.w;
        }
        dest->set_number(component);
        break;
      }
      case RegOp::MakeStruct: {
        if (static_cast<std::size_t>(a) + d > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> fields;
        fields.reserve(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          fields.push_back(m_stack[static_cast<std::size_t>(a) + index]);
        }
        *dest = Value::struct_of(std::move(fields));
        break;
      }
      case RegOp::GetSelf:
      case RegOp::SetSelf: {
        if (d >= m_chunk.constants.size() || !m_chunk.constants[d].is_string() || m_self.table == nullptr) {
          m_error = "runtime error";
          return false;
        }
        const std::string& name = m_chunk.constants[d].text;
        if (op == RegOp::SetSelf) {
          bool replaced = false;
          for (std::pair<std::string, Value>& entry : m_self.table->entries) {
            if (entry.first == name) {
              entry.second = *dest;
              replaced = true;
              break;
            }
          }
          if (!replaced) {
            m_self.table->entries.emplace_back(name, *dest);
          }
          collect_garbage();
          break;
        }
        Value found = Value::number_of(0);
        for (const std::pair<std::string, Value>& entry : m_self.table->entries) {
          if (entry.first == name) {
            found = entry.second;
            break;
          }
        }
        *dest = std::move(found);
        break;
      }
      case RegOp::MakeTask:
        *dest = Value::task_of(*left);
        break;
      case RegOp::Await: {
        if (left == nullptr || !left->is_task()) {
          m_error = "type error";
          return false;
        }
        const Value task = *left;
        if (task.y == 2) {
          if ((task.field_count() == 0)) {
            m_error = "type error";
            return false;
          }
          *dest = task.field(0);
          break;
        }
        Value result = Value::number_of(0);
        if (task.y == 1) {
          if (!finish(static_cast<long long>(task.z), result)) {
            if (recover()) continue;
            return false;
          }
        } else {
          const auto index = schedule_task(task);
          if (index == static_cast<std::size_t>(-1)) { if (recover()) continue; return false; }
          std::shared_ptr<AsyncState> state;
          { std::lock_guard<std::mutex> lock(runtime.mu); state = runtime.tasks[index]; }
          if (m_cooperative && !state->done) {
            m_wait_index = index; m_wait_slot = a; m_suspended = true; return true;
          }
          if (!await_task(index, result)) { if (recover()) continue; return false; }
        }
        *dest = std::move(result);
        break;
      }
      case RegOp::Spawn: {
        if (left == nullptr || !left->is_task()) {
          m_error = "type error";
          return false;
        }
        Value task = *left;
        if (task.y == 0) {
          if (task.number < 0 || task.number >= static_cast<double>(m_chunk.functions.size())) {
            m_error = "runtime error";
            return false;
          }
          const auto index = schedule_task(task);
          if (index == static_cast<std::size_t>(-1)) { if (recover()) continue; return false; }
          task.y = 4;
          task.z = static_cast<double>(index);
        }
        *dest = std::move(task);
        if (m_cooperative) { m_yielded = true; return true; }
        (void)pump_tasks();
        break;
      }
      case RegOp::Parallel: {
        if (d < 2 || static_cast<std::size_t>(a) + d > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> tasks(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          tasks[index] = m_stack[static_cast<std::size_t>(a) + index];
          if (!tasks[index].is_task()) {
            m_error = "type error";
            return false;
          }
        }
        bool pending = false;
        for (std::uint16_t index = 0; index < d; ++index) {
          Value& task = tasks[index];
          if (task.y == 2) {
            continue;
          }
          const auto scheduled = schedule_task(task);
          if (scheduled == static_cast<std::size_t>(-1)) return false;
          task.y = 4; task.z = static_cast<double>(scheduled);
          std::lock_guard<std::mutex> lock(runtime.mu);
          pending = pending || !runtime.tasks[scheduled]->done;
        }
        if (m_cooperative && pending) {
          m_wait_parallel = std::move(tasks); m_wait_slot = a; m_suspended = true; return true;
        }
        // parallel(a(), b(), ...) waits for all and gives their results as a list, in order.
        std::vector<Value> results;
        results.reserve(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          Value result = Value::number_of(0);
          if (tasks[index].y == 2) {
            if ((tasks[index].field_count() == 0)) {
              m_error = "type error";
              return false;
            }
            result = tasks[index].field(0);
          } else if (!await_task(static_cast<std::size_t>(tasks[index].z), result)) {
            if (recover()) {
              break;
            }
            return false;
          }
          results.push_back(std::move(result));
        }
        if (results.size() != d) {
          continue;  // an error was caught
        }
        *dest = Value::struct_of(std::move(results));
        break;
      }
      case RegOp::FileSize:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(stdlib::file_size(left->text));
        break;
      case RegOp::Env:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        *dest = Value::string_of(stdlib::env_value(left->text));
        break;
      case RegOp::HttpHost:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        *dest = Value::string_of(stdlib::http_host(left->text));
        break;
      case RegOp::GetIndex: {
        // References, not copies: `list[i]` used to copy the whole list on every read.
        const Value& object = *left;
        const Value& index = *right;
        if (object.is_struct() && object.number == 65535) {
          Value found;
          if (!object.dictionary_get(index, found)) { m_error = "dictionary key not found: " + format_value(index); if (recover()) continue; return false; }
          *dest = std::move(found);
          break;
        }
        if (!index.is_number()) {
          m_error = "type error";
          return false;
        }
        const auto position = array_index(index);
        if (!position) { m_error = "index out of range: " + format_value(index); if (recover()) continue; return false; }
        const auto at = *position;
        if (object.is_struct()) {
          if (at >= object.field_count()) {
            m_error = "index out of range: " + format_value(index) + " (size " +
                      std::to_string(object.field_count()) + ")";
            if (recover()) {
              continue;
            }
            return false;
          }
          Value element = object.field(at);
          *dest = std::move(element);
          break;
        }
        if (object.is_string()) {
          // One character (a UTF-8 code point), not the byte's number.
          std::size_t offset = 0;
          std::size_t remaining = at;
          const std::string& text = object.text;
          while (offset < text.size() && remaining > 0) {
            offset += utf8_width(text, offset);
            --remaining;
          }
          if (offset >= text.size()) {
            m_error = "index out of range: " + format_value(index);
            if (recover()) {
              continue;
            }
            return false;
          }
          const auto lead = static_cast<unsigned char>(text[offset]);
          const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
          Value character = Value::string_of(text.substr(offset, width));
          *dest = std::move(character);
          break;
        }
        if (!object.is_vector()) {
          m_error = "type error";
          return false;
        }
        if (at >= object.dims) { m_error = "vector index out of range"; if (recover()) continue; return false; }
        double component = 0;
        if (at == 0) {
          component = object.number;
        } else if (at == 1) {
          component = object.y;
        } else if (at == 2) {
          component = object.z;
        } else if (at == 3) {
          component = object.w;
        }
        dest->set_number(component);
        break;
      }
      case RegOp::SpawnThread: {
        if (d >= m_chunk.functions.size() || d >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> args(function.arity);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          args[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        dest->set_integer(launch(d, std::move(args)));
        break;
      }
      case RegOp::Join: {
        const auto handle = integral_value(*left);
        if (!handle) { m_error = "invalid thread handle"; if (recover()) continue; return false; }
        Value result;
        if (!finish(*handle, result)) { if (recover()) continue; return false; }
        *dest = std::move(result);
        break;
      }
      case RegOp::AtomicNew: {
        if (!left->is_integer()) {
          m_error = "type error";
          return false;
        }
        dest->set_integer(make_atomic(runtime, left->integer));
        break;
      }
      case RegOp::FetchAdd: {
        if (!left->is_integer() || !right->is_integer()) {
          m_error = "type error";
          return false;
        }
        long long previous = 0;
        if (!atomic_fetch(runtime, left->integer, right->integer, previous, m_error)) {
          if (m_error.empty()) m_error = "runtime error";
          if (recover()) continue;
          return false;
        }
        dest->set_integer(previous);
        break;
      }
      case RegOp::AtomicLoad: {
        if (!left->is_integer()) {
          m_error = "type error";
          return false;
        }
        long long value = 0;
        if (!atomic_read(runtime, left->integer, value)) {
          if (m_error.empty()) m_error = "runtime error";
          if (recover()) continue;
          return false;
        }
        dest->set_integer(value);
        break;
      }
      case RegOp::MutexNew:
        dest->set_integer(make_mutex(runtime));
        break;
      case RegOp::Lock:
      case RegOp::Unlock: {
        const auto handle = integral_value(*left);
        if (!handle) { m_error = "invalid mutex handle"; if (recover()) continue; return false; }
        RuntimeMutex* cell = mutex_at(runtime, *handle);
        if (cell == nullptr) {
          m_error = "runtime error";
          return false;
        }
        if (op == RegOp::Lock) {
          bool failed = false;
          std::unique_lock<std::mutex> lock(cell->mu);
          while (cell->owner != nullptr) {
            if (cell->owner == m_lock_owner) { m_error = "mutex is already held by this task"; failed = true; break; }
            if (m_cooperative) {
              m_wait_mutex = *handle; --m_ip; m_yielded = true; return true;
            }
            lock.unlock();
            const bool progress = pump_tasks();
            lock.lock();
            if (!progress && cell->owner != nullptr) cell->changed.wait_for(lock, std::chrono::milliseconds(20));
          }
          if (failed) { if (recover()) continue; return false; }
          cell->owner = m_lock_owner;
          m_owned_mutexes.push_back(*handle);
        } else {
          std::lock_guard<std::mutex> lock(cell->mu);
          if (cell->owner != m_lock_owner) { m_error = "mutex is not held by this task"; if (recover()) continue; return false; }
          cell->owner = nullptr; cell->changed.notify_all(); runtime.changed.notify_all();
          m_owned_mutexes.erase(std::remove(m_owned_mutexes.begin(), m_owned_mutexes.end(), *handle), m_owned_mutexes.end());
        }
        dest->set_number(0);
        break;
      }
      case RegOp::ListSort: {
        if (!left->is_struct() || left->number != 0) {
          m_error = "sort expects an array or list";
          if (recover()) continue;
          return false;
        }
        std::vector<Value> fields = left->snapshot_fields();
        bool invalid = false;
        for (const Value& value : fields) {
          if (!value.is_number() || (!value.is_integer() && std::isnan(value.number))) { invalid = true; break; }
        }
        if (invalid) { m_error = "sort requires ordered numeric values (NaN is unordered)"; if (recover()) continue; return false; }
        std::sort(fields.begin(), fields.end(), [](const Value& first, const Value& second) {
          if (first.is_integer() && second.is_integer()) return first.integer < second.integer;
          if (first.is_integer()) return *numeric::compare(first.integer, second.number) < 0;
          if (second.is_integer()) return *numeric::compare(second.integer, first.number) > 0;
          return first.number < second.number;
        });
        *dest = Value::struct_of(std::move(fields));
        break;
      }
      case RegOp::ListFind: {
        if (!left->is_struct() || left->number != 0) {
          m_error = "type error";
          return false;
        }
        if (left->field_count() > static_cast<std::size_t>(numeric::maximum)) {
          m_error = "collection index exceeds number range"; if (recover()) continue; return false;
        }
        std::int64_t found = -1;
        for (std::size_t index = 0; index < left->field_count(); ++index) {
          if (values_equal(left->field(index), *right)) {
            found = static_cast<std::int64_t>(index);
            break;
          }
        }
        dest->set_integer(found);
        break;
      }
      case RegOp::ListLen:
      case RegOp::IterLen: {
        const Value& source = *left;
        if (op == RegOp::IterLen && source.is_number()) {
          const auto count = integral_value(source);
          if (!count) { m_error = "iteration count must be an integer"; if (recover()) continue; return false; }
          dest->set_integer(*count < 0 ? 0 : *count);
          break;
        }
        std::uint64_t count = 0;
        if (source.is_struct()) {
          // dictionaries keep key, value, key, value... and are tagged with 65535
          count = source.number == 65535 ? source.field_count() / 2 : source.field_count();
        } else if (source.is_string()) {
          for (const char byte : source.text) {
            if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) {
              count += 1;
            }
          }
        } else {
          m_error = op == RegOp::ListLen ? "len expects a list, array, dictionary or string"
                                         : "for-in expects a number or a collection";
          if (recover()) {
            continue;
          }
          return false;
        }
        if (count > static_cast<std::uint64_t>(numeric::maximum)) { m_error = "collection size out of range"; if (recover()) continue; return false; }
        dest->set_integer(static_cast<std::int64_t>(count));
        break;
      }
      case RegOp::IterAt: {
        const Value& source = *left;
        const auto position = array_index(*right);
        if (!position) { m_error = "iteration index out of range"; if (recover()) continue; return false; }
        const auto at = *position;
        if (source.is_number()) {
          if (right->is_integer()) dest->set_integer(right->integer);
          else dest->set_number(right->number);
          break;
        }
        if (source.is_struct()) {
          const bool dictionary = source.number == 65535;
          const auto size = dictionary ? source.field_count() / 2 : source.field_count();
          if (at >= size) { m_error = "iteration index out of range"; if (recover()) continue; return false; }
          Value element = source.field(dictionary ? at * 2 : at);
          *dest = std::move(element);
          break;
        }
        // string: the at-th UTF-8 character
        const std::string& text = source.text;
        std::size_t offset = 0;
        for (std::size_t skipped = 0; skipped < at && offset < text.size(); ++skipped) {
          const auto lead = static_cast<unsigned char>(text[offset]);
          offset += lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        }
        const auto lead = static_cast<unsigned char>(offset < text.size() ? text[offset] : 0);
        const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        Value character = Value::string_of(offset < text.size() ? text.substr(offset, width) : std::string());
        *dest = std::move(character);
        break;
      }
      case RegOp::SetField: {
        Value& object = *dest;
        const Value& value = *left;  // B: the new value
        if (object.is_struct()) {
          if (c >= object.field_count()) {
            m_error = "unknown field";
            return false;
          }
          object.set_field(c, value);
          break;
        }
        if (object.is_vector() && c <= 3 && value.is_number()) {
          (c == 0 ? object.number : c == 1 ? object.y : c == 2 ? object.z : object.w) = value.number;
          break;
        }
        m_error = "type error";
        return false;
      }
      case RegOp::SetIndex: {
        Value& object = *dest;
        const Value& index = *left;   // B
        const Value& value = *right;  // C
        if (object.is_struct() && object.number == 65535) {
          object.dictionary_set(index, value);
          break;
        }
        if (!index.is_number()) {
          m_error = "type error";
          return false;
        }
        const auto position = array_index(index);
        if (!position) { m_error = "index out of range: " + format_value(index); if (recover()) continue; return false; }
        const auto at = *position;
        if (object.is_struct()) {
          if (at >= object.field_count()) {
            m_error = "index out of range: " + format_value(index) + " (size " +
                      std::to_string(object.field_count()) + ")";
            if (recover()) {
              continue;
            }
            return false;
          }
          object.set_field(at, value);
          break;
        }
        if (object.is_vector() && at < object.dims && value.is_number()) {
          (at == 0 ? object.number : at == 1 ? object.y : at == 2 ? object.z : object.w) = value.number;
          break;
        }
        m_error = object.is_string() ? "strings cannot be changed in place; build a new one with .:" : "type error";
        if (recover()) {
          continue;
        }
        return false;
      }
      case RegOp::PopItem:
        *dest = left->pop_field();
        break;
      case RegOp::RemoveItem: {
        Value removed;
        if (!left->is_struct()) throw HeapError("remove expects an array or dictionary");
        if (left->number == 65535) {
          if (!left->dictionary_remove(*right, removed)) throw HeapError("dictionary key not found: " + format_value(*right));
        } else {
          const auto position = array_index(*right);
          if (!position || left->number != 0) throw HeapError("invalid array index");
          removed = left->remove_field(*position);
        }
        *dest = std::move(removed); break;
      }
      case RegOp::ListPush:
      case RegOp::ListPop:
      case RegOp::ListInsert:
      case RegOp::ListRemove: {
        Value list = dest->is_cell() ? dest->field(0) : *dest;  // captured collection or ordinary local
        const bool dictionary = list.is_struct() && list.number == 65535;
        if (!list.is_struct() || (dictionary && op != RegOp::ListRemove) || (!dictionary && list.number != 0)) {
          m_error = "push/pop/insert/remove expect a list";
          if (recover()) {
            continue;
          }
          return false;
        }
        if (op == RegOp::ListPush) {
          list.append_field(*left);
          break;
        }
        if (op == RegOp::ListPop) {
          if ((list.field_count() == 0)) {
            m_error = "pop from an empty list";
            if (recover()) {
              continue;
            }
            return false;
          }
          Value last = list.pop_field();
          *left = std::move(last);  // B: the result register
          break;
        }
        if (dictionary) {
          Value removed;
          if (!list.dictionary_remove(*left, removed)) { m_error = "dictionary key not found: " + format_value(*left); if (recover()) continue; return false; }
          *left = std::move(removed);
          break;
        }
        if (!left->is_number()) {
          m_error = "type error";
          return false;
        }
        const auto position = array_index(*left);
        const std::size_t limit = op == RegOp::ListInsert ? list.field_count() + 1 : list.field_count();
        if (!position || *position >= limit || *position > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
          m_error = "index out of range: " + format_value(*left) + " (size " + std::to_string(list.field_count()) + ")";
          if (recover()) {
            continue;
          }
          return false;
        }
        const auto at = static_cast<std::ptrdiff_t>(*position);
        if (op == RegOp::ListInsert) {
          list.insert_field(static_cast<std::size_t>(at), *right);
          break;
        }
        Value removed = list.remove_field(static_cast<std::size_t>(at));
        *left = std::move(removed);
        break;
      }
      case RegOp::IntDiv: {
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        if (right->number == 0) {
          m_error = "division by zero";
          if (recover()) {
            continue;
          }
          return false;
        }
        if (left->is_integer() && right->is_integer()) {
          const auto result = numeric::divide(left->integer, right->integer);
          if (!result) {
            m_error = "integer overflow";
            if (recover()) continue;
            return false;
          }
          dest->set_integer(*result);
        } else {
          const auto result = numeric::from_float(std::trunc(left->number / right->number));
          if (!result) { m_error = "integer overflow"; if (recover()) continue; return false; }
          dest->set_integer(*result);
        }
        break;
      }
      case RegOp::Std: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        if (m_isolated && (c == 4 || c == 5)) {  // fs::read / fs::list: an actor cannot touch files
          m_error = "sandbox";
          if (recover()) {
            continue;
          }
          return false;
        }
        Value copied[8];
        for (std::uint8_t index = 0; index < b; ++index) {
          copied[index] = m_stack[static_cast<std::size_t>(a) + index];
        }
        if (!stdlib::std_apply(c, b == 0 ? nullptr : copied, b, *dest, m_error)) {
          if (recover()) {  // library errors are catchable like any other
            continue;
          }
          return false;
        }
        break;
      }
      case RegOp::Host: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        if (m_isolated && stdlib::host_sandboxed(c)) {  // windows, files, clock: not inside actor(...)
          m_error = "sandbox";
          if (recover()) {
            continue;
          }
          return false;
        }
        Value copied[8];
        for (std::uint8_t index = 0; index < b; ++index) {
          copied[index] = m_stack[static_cast<std::size_t>(a) + index];
        }
        if (!stdlib::host_apply(c, copied, b, *dest, m_error)) {
          if (recover()) {  // library errors are catchable like any other
            continue;
          }
          return false;
        }
        break;
      }
      case RegOp::Axiom: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        Value copied[8];
        for (std::uint8_t index = 0; index < b; ++index) {
          copied[index] = m_stack[static_cast<std::size_t>(a) + index];
        }
        if (!stdlib::axiom_apply(c, copied, b, *dest, m_error)) {
          if (recover()) {  // library errors are catchable like any other
            continue;
          }
          return false;
        }
        break;
      }
      case RegOp::CCall: {
        if (d != 0 || !dest->is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(static_cast<double>(std::strlen(dest->text.c_str())));
        break;
      }
      case RegOp::Protect:
        m_traps.push_back(Trap{d, 0, m_calls.size()});
        break;
      case RegOp::EndTry:
        if (!m_traps.empty()) {
          m_traps.pop_back();
        }
        break;
    }
    } catch (const HeapError& error) {
      m_error = error.what(); if (recover()) continue; return false;
    }
  }
  return m_error.empty();
}

bool VirtualMachine::run_function(const std::uint16_t index, const std::vector<Value>& args, Value& out) {
  SchedulerScope scheduler(*m_runtime);
  std::optional<stdlib::HostExecution> host_execution;
  if (m_runtime_owner) host_execution.emplace();
  struct WorkerCompletion {
    RuntimeContext* runtime;
    VirtualMachine* machine;
    ~WorkerCompletion() { if (runtime) { machine->release_mutexes(); machine->cancel_tasks(); runtime->drain(); runtime->tasks.clear(); } }
  } worker_completion{m_runtime_owner ? m_runtime.get() : nullptr, this};
  if (!transferable_values(args)) { m_error = "legacy table values cannot cross a VM boundary"; return false; }
  m_error.clear();
  m_error_location = {0, 0};
  m_calls.clear();
  m_traps.clear();
  m_heap.clear();
  m_self = Value::table_of(vm::allocate(m_heap));
  const RegChunk lowered = lower_registers(m_chunk);
  if (!lowered.error.empty() || index >= m_chunk.functions.size() || index >= lowered.function_entry.size()) {
    if (m_error.empty()) {
      m_error = lowered.error.empty() ? "runtime error" : lowered.error;
    }
    return false;
  }
  const FunctionBytecode& function = m_chunk.functions[index];
  if (args.size() != function.arity) {
    m_error = "runtime error";
    return false;
  }
  m_calls.push_back(CallFrame{lowered.code.size(), std::vector<Value>(256, Value::number_of(0)), {}, 0, function.arity});
  m_stack.assign(256, Value::number_of(0));
  for (std::size_t arg = 0; arg < args.size(); ++arg) {
    m_stack[arg] = args[arg];
  }
  m_ip = lowered.function_entry[index];
  if (!execute_registers(lowered)) {
    if (m_error_location.line == 0) locate_error(lowered);
    return false;
  }
  if (m_suspended || m_yielded) return true;
  if (m_stack.empty()) {
    m_error = "runtime error";
    return false;
  }
  if (!transferable_values({m_stack[0]})) { m_error = "legacy table values cannot cross a VM boundary"; locate_error(lowered); return false; }
  out = m_stack[0];
  if (!m_cooperative) release_mutexes();
  if (!m_cooperative && (m_runtime_owner || m_complete_on_return) && !complete_tasks()) return false;
  if (m_runtime_owner) {
    m_runtime->drain();
    const auto failure = m_runtime->unobserved_failure();
    if (!failure.first.empty()) { m_error = failure.first; m_error_location = failure.second; }
  }
  return m_error.empty();
}

bool VirtualMachine::run() {
  std::optional<stdlib::HostExecution> host_execution;
  if (m_runtime_owner) host_execution.emplace();
  if (m_runtime_owner) {
    m_runtime->drain();
    m_runtime = std::make_shared<RuntimeContext>();
  }
  SchedulerScope scheduler(*m_runtime);
  struct WorkerCompletion {
    RuntimeContext* runtime;
    VirtualMachine* machine;
    ~WorkerCompletion() { if (runtime) { machine->release_mutexes(); machine->cancel_tasks(); runtime->drain(); runtime->tasks.clear(); } }
  } worker_completion{m_runtime_owner ? m_runtime.get() : nullptr, this};
  m_ip = m_chunk.entry;
  m_stack.clear();
  m_calls.clear();
  m_traps.clear();
  m_error.clear();
  m_error_location = {0, 0};
  m_coroutines.clear();
  m_deferred.clear();
  m_resumer = {};
  m_active = -1;
  m_locals.assign(m_chunk.local_count, Value::number_of(0));
  m_heap.clear();
  m_self = Value::table_of(vm::allocate(m_heap));
  collect_garbage();
  const RegChunk lowered = lower_registers(m_chunk);
  if (!lowered.error.empty()) {
    m_error = lowered.error;
    return false;
  }
  m_stack.assign(256, Value::number_of(0));
  m_locals.clear();
  m_ip = lowered.entry;
  bool ok = execute_registers(lowered);
  if (!ok && m_error_location.line == 0) locate_error(lowered);
  release_mutexes();
  if (ok) ok = complete_tasks();
  if (!ok) cancel_tasks();
  if (m_runtime_owner) {
    m_runtime->drain();
    const auto failure = m_runtime->unobserved_failure();
    if (ok && !failure.first.empty()) {
      m_error = failure.first;
      m_error_location = failure.second;
      ok = false;
    }
    m_runtime->tasks.clear();
  }
  return ok;
}

}  // namespace clpp
