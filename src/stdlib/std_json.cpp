#include <algorithm>
#include <optional>
// @clpp.json — JSON text <-> CL++ values.
//
// Objects become dictionaries (keys in file order), arrays become lists, strings and numbers stay
// themselves. JSON null and booleans preserve their distinct VM value kinds;
// ParseValue/StringifyValue use recursive typed nodes with distinct JSON categories. Struct values are written as
// arrays of their fields (field names do not exist at run time): use a dictionary for objects.

#include "stdlib/host.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::stdlib::host {

namespace {

constexpr double kDictionaryTag = 65535;
constexpr double kListTag = 0;
constexpr double kVariantTag = 65534;
constexpr std::uint8_t kDecimalProxy = 251;
constexpr int kMaxDepth = 256;

class Parser {
 public:
  explicit Parser(std::string_view text, bool typed = false) : m_text(text), m_typed(typed) {}

  bool parse(Value& out, std::string& error) {
    skip_space();
    if (!value(out, 0)) {
      error = message();
      return false;
    }
    skip_space();
    if (m_pos != m_text.size()) {
      m_error = "unexpected text after the value";
      error = message();
      return false;
    }
    return true;
  }

 private:
  [[nodiscard]] std::string message() const {
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t index = 0; index < m_pos && index < m_text.size(); ++index) {
      if (m_text[index] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
    }
    return "json: " + m_error + " at line " + std::to_string(line) + ", column " + std::to_string(column);
  }

  void skip_space() {
    while (m_pos < m_text.size() &&
           (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' || m_text[m_pos] == '\n' || m_text[m_pos] == '\r')) {
      ++m_pos;
    }
  }

  bool expect(const char c) {
    skip_space();
    if (m_pos < m_text.size() && m_text[m_pos] == c) {
      ++m_pos;
      return true;
    }
    m_error = std::string("expected '") + c + "'";
    return false;
  }

  bool literal(const std::string_view word) {
    if (m_text.substr(m_pos, word.size()) == word) {
      m_pos += word.size();
      return true;
    }
    m_error = "invalid value";
    return false;
  }

  bool value(Value& out, const int depth) {
    if (depth > kMaxDepth) {
      m_error = "nesting too deep";
      return false;
    }
    skip_space();
    if (m_pos >= m_text.size()) {
      m_error = "unexpected end of text";
      return false;
    }
    const char c = m_text[m_pos];
    if (c == '{') {
      return object(out, depth);
    }
    if (c == '[') {
      return array(out, depth);
    }
    if (c == '"') {
      std::string text;
      if (!string(text)) {
        return false;
      }
      out = Value::string_of(std::move(text));
      return true;
    }
    if (c == 't') {
      out = Value::boolean_of(true);
      return literal("true");
    }
    if (c == 'f') {
      out = Value::boolean_of(false);
      return literal("false");
    }
    if (c == 'n') {
      out = Value::null_of();
      return literal("null");
    }
    return number(out);
  }

  bool object(Value& out, const int depth) {
    ++m_pos;
    std::vector<Value> fields;
    skip_space();
    if (m_pos < m_text.size() && m_text[m_pos] == '}') {
      ++m_pos;
      out = Value::struct_of({});
      out.number = kDictionaryTag;
      return true;
    }
    for (;;) {
      skip_space();
      if (m_pos >= m_text.size() || m_text[m_pos] != '"') {
        m_error = "expected a key in quotes";
        return false;
      }
      std::string key;
      if (!string(key) || !expect(':')) {
        return false;
      }
      Value item;
      if (!value(item, depth + 1)) {
        return false;
      }
      bool replaced = false;
      for (std::size_t index = 0; index + 1 < fields.size(); index += 2) {
        if (fields[index].text == key) {  // a repeated key keeps the last value
          fields[index + 1] = std::move(item);
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        fields.push_back(Value::string_of(std::move(key)));
        fields.push_back(std::move(item));
      }
      skip_space();
      if (m_pos < m_text.size() && m_text[m_pos] == ',') {
        ++m_pos;
        continue;
      }
      if (!expect('}')) {
        return false;
      }
      break;
    }
    out = Value::struct_of(std::move(fields));
    out.number = kDictionaryTag;
    return true;
  }

  bool array(Value& out, const int depth) {
    ++m_pos;
    std::vector<Value> items;
    skip_space();
    if (m_pos < m_text.size() && m_text[m_pos] == ']') {
      ++m_pos;
      out = Value::struct_of({});
      out.number = kListTag;
      return true;
    }
    for (;;) {
      Value item;
      if (!value(item, depth + 1)) {
        return false;
      }
      items.push_back(std::move(item));
      skip_space();
      if (m_pos < m_text.size() && m_text[m_pos] == ',') {
        ++m_pos;
        continue;
      }
      if (!expect(']')) {
        return false;
      }
      break;
    }
    out = Value::struct_of(std::move(items));
    out.number = kListTag;
    return true;
  }

  static void append_utf8(std::string& out, const std::uint32_t code) {
    if (code < 0x80) {
      out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
      out.push_back(static_cast<char>(0xC0U | (code >> 6U)));
      out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else if (code < 0x10000) {
      out.push_back(static_cast<char>(0xE0U | (code >> 12U)));
      out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
      out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else {
      out.push_back(static_cast<char>(0xF0U | (code >> 18U)));
      out.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3FU)));
      out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
      out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    }
  }

  bool hex4(std::uint32_t& code) {
    if (m_pos + 4 > m_text.size()) {
      m_error = "incomplete \\u escape";
      return false;
    }
    code = 0;
    for (int index = 0; index < 4; ++index) {
      const char c = m_text[m_pos++];
      code <<= 4U;
      if (c >= '0' && c <= '9') {
        code |= static_cast<std::uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        code |= static_cast<std::uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        code |= static_cast<std::uint32_t>(c - 'A' + 10);
      } else {
        m_error = "invalid \\u escape";
        return false;
      }
    }
    return true;
  }

  bool string(std::string& out) {
    ++m_pos;  // opening quote
    while (m_pos < m_text.size()) {
      const char c = m_text[m_pos++];
      if (c == '"') {
        return true;
      }
      if (static_cast<unsigned char>(c) < 0x20) {
        m_error = "control character inside a string";
        return false;
      }
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (m_pos >= m_text.size()) {
        break;
      }
      const char escape = m_text[m_pos++];
      switch (escape) {
        case '"':
        case '\\':
        case '/':
          out.push_back(escape);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          std::uint32_t code = 0;
          if (!hex4(code)) {
            return false;
          }
          if (code >= 0xD800 && code <= 0xDBFF && m_text.substr(m_pos, 2) == "\\u") {
            m_pos += 2;
            std::uint32_t low = 0;
            if (!hex4(low)) {
              return false;
            }
            if (low < 0xDC00 || low > 0xDFFF) { m_error = "invalid Unicode surrogate pair"; return false; }
            code = 0x10000U + ((code - 0xD800U) << 10U) + (low - 0xDC00U);
          }
          if (code >= 0xD800 && code <= 0xDFFF) { m_error = "unpaired Unicode surrogate"; return false; }
          append_utf8(out, code);
          break;
        }
        default:
          m_error = std::string("invalid escape \\") + escape;
          return false;
      }
    }
    m_error = "unterminated string";
    return false;
  }

  bool number(Value& out) {
    const std::size_t start = m_pos;
    const auto digit = [&](const char low, const char high) {
      return m_pos < m_text.size() && m_text[m_pos] >= low && m_text[m_pos] <= high;
    };
    const auto invalid = [&]() {
      m_pos = start;
      m_error = "invalid number";
      return false;
    };
    if (m_pos < m_text.size() && m_text[m_pos] == '-') ++m_pos;
    if (digit('0', '0')) {
      ++m_pos;
      if (digit('0', '9')) return invalid();
    } else {
      if (!digit('1', '9')) return invalid();
      do { ++m_pos; } while (digit('0', '9'));
    }
    if (m_pos < m_text.size() && m_text[m_pos] == '.') {
      ++m_pos;
      if (!digit('0', '9')) return invalid();
      do { ++m_pos; } while (digit('0', '9'));
    }
    if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
      ++m_pos;
      if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
      if (!digit('0', '9')) return invalid();
      do { ++m_pos; } while (digit('0', '9'));
    }
    double parsed = 0;
    const char* begin = m_text.data() + start;
    const char* end = m_text.data() + m_pos;
    if (m_text.substr(start, m_pos - start).find_first_of(".eE") == std::string_view::npos) {
      std::int64_t integer = 0;
      const auto converted = std::from_chars(begin, end, integer);
      if (converted.ec != std::errc{} || converted.ptr != end) return invalid();
      out = Value::integer_of(integer); return true;
    }
    const auto converted = std::from_chars(begin, end, parsed, std::chars_format::general);
    if (converted.ec != std::errc{} || converted.ptr != end || !std::isfinite(parsed)) return invalid();
    out = Value::number_of(parsed);
    return true;
  }

  std::string_view m_text;
  bool m_typed{false};
  std::size_t m_pos{0};
  std::string m_error;
};

void write_number(std::string& out, const double number) {
  if (!std::isfinite(number)) {
    out += "null";
    return;
  }
  if (number == std::trunc(number) && std::fabs(number) < 1e15) {
    out += std::to_string(static_cast<long long>(number));
    return;
  }
  char buffer[64];
  const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), number);
  out.append(buffer, result.ptr);
}

void write_string(std::string& out, const std::string_view text) {
  out.push_back('"');
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(kHex[(static_cast<unsigned char>(c) >> 4U) & 0xFU]);
          out.push_back(kHex[static_cast<unsigned char>(c) & 0xFU]);
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  out.push_back('"');
}

[[nodiscard]] std::string key_text(const Value& key) {
  if (key.kind == Value::Kind::Integer) return std::to_string(key.integer);
  if (key.is_string()) {
    return key.text;
  }
  std::string text;
  write_number(text, key.number);
  return text;
}

void write(std::string& out, const Value& value, const int indent, const int depth) {
  static thread_local std::vector<const void*> path;
  struct Guard { std::vector<const void*>& path; bool active; ~Guard() { if (active) path.pop_back(); } };
  if (value.is_struct() && std::find(path.begin(),path.end(),value.identity())!=path.end()) throw HeapError("JSON cannot encode cyclic values");
  const bool active=value.is_struct(); if (active) path.push_back(value.identity());
  const Guard guard{path,active};

  const auto newline = [&](const int level) {
    if (indent > 0) {
      out.push_back('\n');
      out.append(static_cast<std::size_t>(indent * level), ' ');
    }
  };
  if (depth > kMaxDepth) {
    out += "null";
    return;
  }
  switch (value.kind) {
    case Value::Kind::Boolean:
      out += value.number != 0 ? "true" : "false";
      return;
    case Value::Kind::Null:
      out += "null";
      return;
    case Value::Kind::Integer:
      out += std::to_string(value.integer);
      return;
    case Value::Kind::Number:
      write_number(out, value.number);
      return;
    case Value::Kind::String:
      write_string(out, value.text);
      return;
    case Value::Kind::Vector: {
      const double parts[4] = {value.number, value.y, value.z, value.w};
      out.push_back('[');
      for (int index = 0; index < value.dims && index < 4; ++index) {
        if (index != 0) {
          out += indent > 0 ? ", " : ",";
        }
        write_number(out, parts[index]);
      }
      out.push_back(']');
      return;
    }
    case Value::Kind::Struct: {
      // Internal proxies never escape the typed JSON adapter into the source VM.
      if (value.dims == kDecimalProxy && value.field_count() == 1) {
        const auto number = value.field(0).number;
        if (number == 0 && std::signbit(number)) { out += "-0.0"; return; }
        const auto start = out.size(); write_number(out, number);
        if (out.find_first_of(".eE", start) == std::string::npos) out += ".0";
        return;
      }
      if (value.number == kVariantTag && value.field_count() == 1) {
        write(out, value.field(0), indent, depth + 1);
        return;
      }
      if (value.number == kDictionaryTag) {
        if ((value.field_count() == 0)) {
          out += "{}";
          return;
        }
        out.push_back('{');
        for (std::size_t index = 0; index + 1 < value.field_count(); index += 2) {
          if (index != 0) {
            out.push_back(',');
          }
          newline(depth + 1);
          write_string(out, key_text(value.field(index)));
          out += indent > 0 ? ": " : ":";
          write(out, value.field(index + 1), indent, depth + 1);
        }
        newline(depth);
        out.push_back('}');
        return;
      }
      if ((value.field_count() == 0)) {
        out += "[]";
        return;
      }
      out.push_back('[');
      for (std::size_t index = 0; index < value.field_count(); ++index) {
        if (index != 0) {
          out.push_back(',');
        }
        newline(depth + 1);
        write(out, value.field(index), indent, depth + 1);
      }
      newline(depth);
      out.push_back(']');
      return;
    }
    case Value::Kind::Table: {
      if (value.table == nullptr || value.table->entries.empty()) {
        out += "{}";
        return;
      }
      out.push_back('{');
      bool first = true;
      for (const auto& [key, item] : value.table->entries) {
        if (!first) {
          out.push_back(',');
        }
        first = false;
        newline(depth + 1);
        write_string(out, key);
        out += indent > 0 ? ": " : ":";
        write(out, item, indent, depth + 1);
      }
      newline(depth);
      out.push_back('}');
      return;
    }
    default:
      out += "null";
      return;
  }
}

// One step of a path: a dictionary key, or a list index.
[[nodiscard]] std::optional<Value> child(const Value& value, const std::string& step) {
  if (!value.is_struct()) {
    return std::nullopt;
  }
  if (value.number == kDictionaryTag) {
    for (std::size_t index = 0; index + 1 < value.field_count(); index += 2) {
      if (key_text(value.field(index)) == step) {
        return value.field(index + 1);
      }
    }
    return std::nullopt;
  }
  if (step.empty() || step.find_first_not_of("0123456789") != std::string::npos) {
    return std::nullopt;
  }
  std::size_t index = 0;
  const auto converted = std::from_chars(step.data(), step.data() + step.size(), index);
  if (converted.ec != std::errc{} || converted.ptr != step.data() + step.size()) return std::nullopt;
  return index < value.field_count() ? std::optional<Value>{value.field(index)} : std::nullopt;
}

[[nodiscard]] std::optional<Value> follow(const Value& root, const std::string& path) {
  std::optional<Value> current = root;
  std::size_t start = 0;
  while (current.has_value() && start <= path.size() && !path.empty()) {
    const std::size_t dot = path.find('.', start);
    const std::string step = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
    current = child(*current, step);
    if (dot == std::string::npos) {
      break;
    }
    start = dot + 1;
  }
  return current;
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(json_parse) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Json.Parse: expected a string");
  }
  return Parser(args[0].text).parse(out, error);
}

CLPP_NATIVE(json_valid) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Json.Valid: expected a string");
  }
  Value ignored;
  std::string reason;
  out = boolean(Parser(args[0].text).parse(ignored, reason));
  return true;
}

CLPP_NATIVE(json_stringify) {  // (value, indent)
  (void)error;
  std::string text;
  try { write(text, args[0], arity == 2 ? static_cast<int>(num(args[1])) : 0, 0); }
  catch (const HeapError& failure) { return fail(error, failure.what()); }
  out = Value::string_of(std::move(text));
  return true;
}

CLPP_NATIVE(json_get) {
  if (arity != 2 || !args[1].is_string()) {
    return fail(error, "Json.Get: expected (value, string path)");
  }
  const auto found = follow(args[0], args[1].text);
  if (!found.has_value()) {
    return fail(error, "Json.Get: nothing at \"" + args[1].text + "\"");
  }
  out = *found;
  return true;
}

CLPP_NATIVE(json_has) {
  if (arity != 2 || !args[1].is_string()) {
    return fail(error, "Json.Has: expected (value, string path)");
  }
  out = boolean(follow(args[0], args[1].text).has_value());
  return true;
}

CLPP_NATIVE(json_set) {  // (object or list, key or index, value) -> changed copy
  (void)arity;
  Value result = Value::struct_of(args[0].snapshot_fields());
  result.number = args[0].number;
  if (!result.is_struct() || (result.number != kDictionaryTag && result.number != kListTag)) {
    return fail(error, "Json.Set: the first argument must be a dictionary or a list");
  }
  if (result.number == kListTag) {
    if (!args[1].is_number() || !std::isfinite(args[1].number) || std::trunc(args[1].number) != args[1].number ||
        args[1].number < 0 || args[1].number > static_cast<double>(result.field_count())) {
      return fail(error, "Json.Set: list index out of range");
    }
    const auto index = static_cast<std::size_t>(args[1].number);
    if (index == result.field_count()) {
      result.append_field(args[2]);
    } else {
      result.set_field(index, args[2]);
    }
    out = std::move(result);
    return true;
  }
  const std::string key = key_text(args[1]);
  for (std::size_t index = 0; index + 1 < result.field_count(); index += 2) {
    if (key_text(result.field(index)) == key) {
      result.set_field(index + 1, args[2]);
      out = std::move(result);
      return true;
    }
  }
  result.append_field(Value::string_of(key));
  result.append_field(args[2]);
  out = std::move(result);
  return true;
}

CLPP_NATIVE(json_object) {
  (void)args;
  (void)arity;
  (void)error;
  out = Value::struct_of({});
  out.number = kDictionaryTag;
  return true;
}

CLPP_NATIVE(json_keys) {
  (void)arity;
  if (!args[0].is_struct() || args[0].number != kDictionaryTag) {
    return fail(error, "Json.Keys: expected a dictionary");
  }
  std::vector<Value> keys;
  for (std::size_t index = 0; index + 1 < args[0].field_count(); index += 2) {
    keys.push_back(Value::string_of(key_text(args[0].field(index))));
  }
  out = Value::struct_of(std::move(keys));
  out.number = kListTag;
  return true;
}

Value typed_node(const Value& raw, const Value& prototype, const int depth) {
  if (depth > kMaxDepth) throw HeapError("JSON nesting limit exceeded");
  auto items = Value::struct_of({}); auto entries = Value::struct_of({}); entries.number = kDictionaryTag;
  std::vector<Value> fields{Value::integer_of(0), Value::string_of(""), Value::boolean_of(false), Value::integer_of(0), Value::number_of(0), items, entries};
  if (raw.is_boolean()) { fields[0] = Value::integer_of(1); fields[2] = raw; }
  else if (raw.kind == Value::Kind::Integer) { fields[0] = Value::integer_of(2); fields[3] = raw; }
  else if (raw.kind == Value::Kind::Number) { fields[0] = Value::integer_of(3); fields[4] = raw; }
  else if (raw.is_string()) { fields[0] = Value::integer_of(4); fields[1] = raw; }
  else if (raw.is_struct() && raw.number == kDictionaryTag) {
    fields[0] = Value::integer_of(6); const auto source = raw.snapshot_fields();
    for (std::size_t index = 0; index < source.size(); index += 2) { entries.append_field(source[index]); entries.append_field(typed_node(source[index + 1], prototype, depth + 1)); }
  } else if (raw.is_struct()) {
    fields[0] = Value::integer_of(5);
    for (const auto& item : raw.snapshot_fields()) items.append_field(typed_node(item, prototype, depth + 1));
  } else if (!raw.is_null()) throw HeapError("unsupported JSON value");
  auto result = Value::struct_of(std::move(fields)); result.number = prototype.number; return result;
}

Value untyped_node(const Value& node, const double owner, std::vector<const void*>& path, const int depth) {
  if (depth > kMaxDepth) throw HeapError("JSON nesting limit exceeded");
  if (!node.is_struct() || node.number != owner || node.field_count() != 7) throw HeapError("invalid typed JSON value");
  if (std::find(path.begin(), path.end(), node.identity()) != path.end()) throw HeapError("JSON cannot encode cyclic values");
  path.push_back(node.identity()); struct Restore { std::vector<const void*>& path; ~Restore() { path.pop_back(); } } restore{path};
  const auto fields = node.snapshot_fields();
  if (fields[0].kind != Value::Kind::Integer) throw HeapError("invalid typed JSON kind");
  switch (fields[0].integer) {
    case 0: return Value::null_of();
    case 1: {
      if (!fields[2].is_boolean() || (fields[2].number != 0 && fields[2].number != 1)) throw HeapError("invalid JSON boolean");
      return fields[2];
    }
    case 2: if (fields[3].kind != Value::Kind::Integer) throw HeapError("invalid JSON integer"); return fields[3];
    case 3: {
      if (!fields[4].is_number() || !std::isfinite(fields[4].number)) throw HeapError("invalid JSON decimal");
      auto value = Value::struct_of({fields[4]}); value.dims = kDecimalProxy; return value;
    }
    case 4: if (!fields[1].is_string()) throw HeapError("invalid JSON text"); return fields[1];
    case 5: {
      if (!fields[5].is_struct() || fields[5].number != kListTag) throw HeapError("invalid JSON array");
      auto value = Value::struct_of({});
      for (const auto& item : fields[5].snapshot_fields()) value.append_field(untyped_node(item, owner, path, depth + 1));
      return value;
    }
    case 6: {
      if (!fields[6].is_struct() || fields[6].number != kDictionaryTag || fields[6].field_count() % 2) throw HeapError("invalid JSON object");
      auto value = Value::struct_of({}); value.number = kDictionaryTag; const auto entries = fields[6].snapshot_fields();
      for (std::size_t index = 0; index < entries.size(); index += 2) {
        if (!entries[index].is_string()) throw HeapError("JSON object keys must be strings");
        value.append_field(entries[index]); value.append_field(untyped_node(entries[index + 1], owner, path, depth + 1));
      }
      return value;
    }
    default: throw HeapError("invalid typed JSON kind");
  }
}

CLPP_NATIVE(json_parse_value) {
  if (arity != 2 || !args[0].is_string() || !args[1].is_struct() || args[1].field_count() != 7) return fail(error, "Json.ParseValue: expected text and value prototype");
  Value raw; if (!Parser(args[0].text, true).parse(raw, error)) return false;
  try { out = typed_node(raw, args[1], 0); return true; } catch (const HeapError& failure) { return fail(error, failure.what()); }
}

CLPP_NATIVE(json_stringify_value) {
  if (arity != 2 || !args[1].is_struct() || args[1].field_count() != 7 || args[1].number <= 0 || args[1].number >= kVariantTag) return fail(error, "Json.StringifyValue: expected a typed value prototype");
  try { std::vector<const void*> path; const auto raw = untyped_node(args[0], args[1].number, path, 0); std::string text; write(text, raw, 0, 0); out = Value::string_of(std::move(text)); return true; }
  catch (const HeapError& failure) { return fail(error, failure.what()); }
}

#undef CLPP_NATIVE

constexpr std::string_view kJsonSource = R"clp(<< @clpp.json: JSON text <-> CL++ values.
<< ParseValue/StringifyValue use recursive typed Value nodes. Parse/Get remain dynamic compatibility APIs.

export enum class Kind { Null, Boolean, Integer, Decimal, Text, Array, Object }
export final def Value {
  Kind Type = Kind::Null;
  string Text = "";
  bool Boolean = false;
  number Integer = 0;
  float Decimal = 0.0;
  array<Value> Items = array<Value>();
  dictionary<string,Value> Entries = dictionary<string,Value>();
}
export Value ParseValue(string text) { return json::ParseValue(text, Value {}); }
export string StringifyValue(Value value) { return json::StringifyValue(value, Value {}); }

export any Parse(string text) { return json::Parse(text); }
export bool Valid(string text) { return json::Valid(text); }
export string Stringify(any value) { return json::Stringify(value); }
export string Pretty(any value) { return json::Stringify(value, 2); }
export any Get(any value, string path) { return json::Get(value, path); }
export bool Has(any value, string path) { return json::Has(value, path); }
export any Set(any target, any key, any value) { return json::Set(target, key, value); }
export any Object() { return json::Object(); }
export any Keys(any object) { return json::Keys(object); }
)clp";

}  // namespace

void add_json(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"json::Parse", 1, json_parse, false},
                                {"json::Valid", 1, json_valid, false},
                                {"json::Stringify", 1, json_stringify, false},
                                {"json::Stringify", 2, json_stringify, false},
                                {"json::Get", 2, json_get, false},
                                {"json::Has", 2, json_has, false},
                                {"json::Set", 3, json_set, false},
                                {"json::Object", 0, json_object, false},
                                {"json::Keys", 1, json_keys, false},
                            });
}

std::string_view json_source() { return kJsonSource; }

void add_json_typed(std::vector<Entry>& table) {
  table.push_back({"json::ParseValue", 2, json_parse_value, false});
  table.push_back({"json::StringifyValue", 2, json_stringify_value, false});
}

}  // namespace clpp::stdlib::host
