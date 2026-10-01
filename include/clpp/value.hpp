#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace clpp {

struct Table;

struct Value {
  enum class Kind { Number, String, Vector, Buffer, Struct, Table, Task };

  Kind kind{Kind::Number};
  std::uint8_t dims{3};  // vectors: 2, 3 or 4 components (only used for printing); sits in padding
  double number{0};
  double y{0};
  double z{0};
  double w{0};
  std::string text;
  std::vector<Value> fields;
  Table* table{nullptr};

  Value() = default;
  Value(const Value&) = default;
  Value(Value&&) noexcept = default;
  Value& operator=(Value&&) noexcept = default;
  ~Value() = default;

  // Numbers are by far the most common values; copying one must not touch the string and the
  // field vector (profiling showed those copies and frees at ~50% of an arithmetic loop).
  // Invariant: a Number never owns text or fields.
  Value& operator=(const Value& other) {
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

  void assign_slow(const Value& other) {
    kind = other.kind;
    dims = other.dims;
    number = other.number;
    y = other.y;
    z = other.z;
    w = other.w;
    text = other.text;
    fields = other.fields;
    table = other.table;
  }

  void clear_payload() {
    kind = Kind::Number;
    y = 0;
    z = 0;
    w = 0;
    text.clear();
    fields.clear();
    table = nullptr;
  }

  [[nodiscard]] static Value number_of(const double value) {
    Value result;
    result.kind = Kind::Number;
    result.number = value;
    return result;
  }

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

  [[nodiscard]] static Value struct_of(std::vector<Value> values) {
    Value result;
    result.kind = Kind::Struct;
    result.fields = std::move(values);
    return result;
  }

  [[nodiscard]] static Value table_of(Table* table) {
    Value result;
    result.kind = Kind::Table;
    result.table = table;
    return result;
  }

  [[nodiscard]] bool is_number() const { return kind == Kind::Number; }
  [[nodiscard]] bool is_string() const { return kind == Kind::String; }
  [[nodiscard]] bool is_vector() const { return kind == Kind::Vector; }
  [[nodiscard]] bool is_buffer() const { return kind == Kind::Buffer; }
  [[nodiscard]] bool is_struct() const { return kind == Kind::Struct; }
  [[nodiscard]] static Value task_of(Value inner) {
    Value result;
    result.kind = Kind::Task;
    result.y = 2;
    result.fields.push_back(std::move(inner));
    return result;
  }

  [[nodiscard]] bool is_table() const { return kind == Kind::Table; }
  [[nodiscard]] bool is_task() const { return kind == Kind::Task; }
};

struct Table {
  bool marked{false};
  std::vector<std::pair<std::string, Value>> entries;
};

}  // namespace clpp
