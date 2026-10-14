#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace clpp {

struct BytecodeChunk;
struct Table;
struct HeapObject;
struct HeapArena;
class HeapError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
struct HeapStats { std::size_t objects{0}; std::size_t frozen{0}; };
[[nodiscard]] HeapStats shared_heap_stats();
std::size_t collect_shared_heap();

struct Value {
  enum class Kind { Number, String, Vector, Buffer, Struct, Table, Task, Null, Integer, Closure, Cell, Signal, Connection, Boolean };

  Kind kind{Kind::Number};
  std::uint8_t dims{3};  // vectors: 2, 3 or 4 components (only used for printing); sits in padding
  double number{0};
  std::int64_t integer{0};
  double y{0};
  double z{0};
  double w{0};
  std::string text;
  Table* table{nullptr};

  Value() = default;
  Value(const Value& other) {
    if (other.kind == Kind::Integer) set_integer(other.integer);
    else if (other.kind == Kind::Number) number = other.number;
    else assign_slow(other);
  }
  Value(Value&& other) noexcept { *this = std::move(other); }
  Value& operator=(Value&& other) noexcept {
    if (this == &other) return *this;
    if (other.kind == Kind::Integer) { set_integer(other.integer); other.kind = Kind::Number; other.number = 0; }
    else if (other.kind == Kind::Number) { set_number(other.number); other.number = 0; }
    else move_slow(other);
    return *this;
  }
  ~Value();

  // Numbers are by far the most common values; copying one must not touch the string and the
  // managed object storage. Scalar assignment stays independent of heap locking.
  // Invariant: a Number never owns text or fields.
  Value& operator=(const Value& other) {
    if (other.kind == Kind::Integer) { set_integer(other.integer); return *this; }
    if (other.kind == Kind::Number) {
      set_number(other.number);
      return *this;
    }
    if (this != &other) {
      assign_slow(other);
    }
    return *this;
  }

  // Turn this value into a number in place, releasing any string or fields it held.
  void set_number(const double value) {
    if (kind != Kind::Number) {
      clear_payload();
    }
    number = value;
  }

  void assign_slow(const Value& other);
  void clear_payload();
  [[nodiscard]] std::size_t field_count() const;
  [[nodiscard]] Value field(std::size_t index) const;
  [[nodiscard]] std::vector<Value> snapshot_fields() const;
  void set_field(std::size_t index, const Value& value);
  void append_field(const Value& value);
  void insert_field(std::size_t index, const Value& value);
  [[nodiscard]] Value remove_field(std::size_t index);
  [[nodiscard]] Value pop_field();
  [[nodiscard]] bool dictionary_get(const Value& key, Value& out) const;
  void dictionary_set(const Value& key, const Value& value);
  [[nodiscard]] bool dictionary_remove(const Value& key, Value& out);
  void freeze() const;
  [[nodiscard]] bool frozen() const;
  [[nodiscard]] const void* identity() const;
  [[nodiscard]] bool same_value(const Value& other) const;

  [[nodiscard]] static Value null_of() {
    Value result;
    result.kind = Kind::Null;
    return result;
  }

  [[nodiscard]] bool is_null() const { return kind == Kind::Null; }

  void set_boolean(const bool value) {
    if (kind != Kind::Boolean) clear_payload();
    kind = Kind::Boolean; number = value ? 1 : 0; integer = 0;
  }
  [[nodiscard]] static Value boolean_of(const bool value) {
    Value result; result.set_boolean(value); return result;
  }
  [[nodiscard]] bool is_boolean() const { return kind == Kind::Boolean; }

  [[nodiscard]] static Value number_of(const double value) {
    Value result;
    result.kind = Kind::Number;
    result.number = value;
    return result;
  }

  void set_integer(const std::int64_t value) {
    if (kind != Kind::Integer && kind != Kind::Number) clear_payload();
    kind = Kind::Integer;
    integer = value;
    number = static_cast<double>(value);  // Existing native ABI; exact integer operations use integer.
  }

  [[nodiscard]] static Value integer_of(const std::int64_t value) {
    Value result;
    result.set_integer(value);
    return result;
  }

  [[nodiscard]] bool is_integer() const { return kind == Kind::Integer; }

  [[nodiscard]] static Value string_of(std::string value) {
    Value result;
    result.kind = Kind::String;
    result.text = std::move(value);
    return result;
  }

  [[nodiscard]] static Value vector_of(const double x, const double y_value, const double z_value, const double w_value = 0,
                                       const std::uint8_t dimensions = 3) {
    Value result;
    result.kind = Kind::Vector;
    result.dims = dimensions;
    result.number = x;
    result.y = y_value;
    result.z = z_value;
    result.w = w_value;
    return result;
  }

  [[nodiscard]] static Value buffer_of(const double size) {
    Value result;
    result.kind = Kind::Buffer;
    result.number = size;
    return result;
  }

  [[nodiscard]] static Value struct_of(std::vector<Value> values);
  [[nodiscard]] static Value cell_of(Value value);
  [[nodiscard]] static Value closure_of(std::uint16_t function, std::vector<Value> cells,
                                        std::shared_ptr<const BytecodeChunk> code);
  [[nodiscard]] std::shared_ptr<const BytecodeChunk> closure_code() const;
  [[nodiscard]] bool is_closure() const { return kind == Kind::Closure; }
  [[nodiscard]] bool is_cell() const { return kind == Kind::Cell; }
  [[nodiscard]] bool is_resource() const { return kind == Kind::Signal || kind == Kind::Connection; }
  [[nodiscard]] static Value signal_of();
  [[nodiscard]] static Value connection_of(Value callback, bool once, bool managed);
  [[nodiscard]] bool connected() const;
  [[nodiscard]] Value subscribe(Value callback, bool once, bool managed = false);
  [[nodiscard]] std::vector<Value> connections();
  [[nodiscard]] Value take_callback();
  void disconnect();
  void clear_managed();

  [[nodiscard]] static Value table_of(Table* table) {
    Value result;
    result.kind = Kind::Table;
    result.table = table;
    return result;
  }

  [[nodiscard]] bool is_number() const { return kind == Kind::Number || kind == Kind::Integer; }
  [[nodiscard]] bool is_string() const { return kind == Kind::String; }
  [[nodiscard]] bool is_vector() const { return kind == Kind::Vector; }
  [[nodiscard]] bool is_buffer() const { return kind == Kind::Buffer; }
  [[nodiscard]] bool is_struct() const { return kind == Kind::Struct; }
  [[nodiscard]] static Value task_of(Value inner);
  [[nodiscard]] static Value task_fields(std::vector<Value> values);
  [[nodiscard]] static Value task_fields(std::vector<Value> values, std::shared_ptr<const BytecodeChunk> code);
  [[nodiscard]] std::shared_ptr<const BytecodeChunk> task_code() const;

  [[nodiscard]] bool is_table() const { return kind == Kind::Table; }
  [[nodiscard]] bool is_task() const { return kind == Kind::Task; }
 private:
  friend struct HeapArena;
  std::shared_ptr<HeapObject> m_object;
  std::shared_ptr<HeapArena> m_arena; // external root ownership; interior edges do not retain the arena
  void reset_reference();
  void move_slow(Value& other) noexcept;
  [[nodiscard]] static Value make_fields(std::vector<Value> values, Kind kind);
  [[nodiscard]] std::shared_ptr<HeapArena> arena() const;
  [[nodiscard]] static Value interior(const Value& value);
};

struct Table {
  bool marked{false};
  std::vector<std::pair<std::string, Value>> entries;
};

}  // namespace clpp
