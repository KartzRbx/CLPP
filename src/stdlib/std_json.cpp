// @clpp.json — JSON text <-> CL++ values.
//
// Objects become dictionaries (keys in file order), arrays become lists, strings and numbers stay
// themselves. CL++ has no separate boolean or null value at run time, so `true`/`false` become 1/0
// and `null` becomes 0; when writing JSON, numbers stay numbers. Struct values are written as
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
constexpr int kMaxDepth = 256;

class Parser {
 public:
  explicit Parser(std::string_view text) : m_text(text) {}

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
      out = Value::number_of(1);
      return literal("true");
    }
    if (c == 'f') {
      out = Value::number_of(0);
      return literal("false");
    }
    if (c == 'n') {
      out = Value::number_of(0);
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
            code = 0x10000U + ((code - 0xD800U) << 10U) + (low - 0xDC00U);
          }
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
    if (m_pos < m_text.size() && m_text[m_pos] == '-') {
      ++m_pos;
    }
    while (m_pos < m_text.size() && (std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0 || m_text[m_pos] == '.' ||
                                     m_text[m_pos] == 'e' || m_text[m_pos] == 'E' || m_text[m_pos] == '+' ||
                                     m_text[m_pos] == '-')) {
      ++m_pos;
    }
    const std::string piece(m_text.substr(start, m_pos - start));
    if (piece.empty() || piece == "-") {
      m_pos = start;
      m_error = "invalid value";
      return false;
    }
    char* end = nullptr;
    const double parsed = std::strtod(piece.c_str(), &end);
    if (end == nullptr || *end != '\0') {
      m_pos = start;
      m_error = "invalid number";
      return false;
    }
    out = Value::number_of(parsed);
    return true;
  }

  std::string_view m_text;
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
  if (key.is_string()) {
    return key.text;
  }
  std::string text;
  write_number(text, key.number);
  return text;
}

void write(std::string& out, const Value& value, const int indent, const int depth) {
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
      if (value.number == kVariantTag && value.fields.size() == 1) {
        write(out, value.fields[0], indent, depth + 1);
        return;
      }
      if (value.number == kDictionaryTag) {
        if (value.fields.empty()) {
          out += "{}";
          return;
        }
        out.push_back('{');
        for (std::size_t index = 0; index + 1 < value.fields.size(); index += 2) {
          if (index != 0) {
            out.push_back(',');
          }
          newline(depth + 1);
          write_string(out, key_text(value.fields[index]));
          out += indent > 0 ? ": " : ":";
          write(out, value.fields[index + 1], indent, depth + 1);
        }
        newline(depth);
        out.push_back('}');
        return;
      }
      if (value.fields.empty()) {
        out += "[]";
        return;
      }
      out.push_back('[');
      for (std::size_t index = 0; index < value.fields.size(); ++index) {
        if (index != 0) {
          out.push_back(',');
        }
        newline(depth + 1);
        write(out, value.fields[index], indent, depth + 1);
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
[[nodiscard]] const Value* child(const Value& value, const std::string& step) {
  if (!value.is_struct()) {
    return nullptr;
  }
  if (value.number == kDictionaryTag) {
    for (std::size_t index = 0; index + 1 < value.fields.size(); index += 2) {
      if (key_text(value.fields[index]) == step) {
        return &value.fields[index + 1];
      }
    }
    return nullptr;
  }
  if (step.empty() || step.find_first_not_of("0123456789") != std::string::npos) {
    return nullptr;
  }
  const std::size_t index = std::stoul(step);
  return index < value.fields.size() ? &value.fields[index] : nullptr;
}

[[nodiscard]] const Value* follow(const Value& root, const std::string& path) {
  const Value* current = &root;
  std::size_t start = 0;
  while (current != nullptr && start <= path.size() && !path.empty()) {
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
  write(text, args[0], arity == 2 ? static_cast<int>(num(args[1])) : 0, 0);
  out = Value::string_of(std::move(text));
  return true;
}

CLPP_NATIVE(json_get) {
  if (arity != 2 || !args[1].is_string()) {
    return fail(error, "Json.Get: expected (value, string path)");
  }
  const Value* found = follow(args[0], args[1].text);
  if (found == nullptr) {
    return fail(error, "Json.Get: nothing at \"" + args[1].text + "\"");
  }
  out = *found;
  return true;
}

CLPP_NATIVE(json_has) {
  if (arity != 2 || !args[1].is_string()) {
    return fail(error, "Json.Has: expected (value, string path)");
  }
  out = boolean(follow(args[0], args[1].text) != nullptr);
  return true;
}

CLPP_NATIVE(json_set) {  // (object or list, key or index, value) -> changed copy
  (void)arity;
  Value result = args[0];
  if (!result.is_struct() || (result.number != kDictionaryTag && result.number != kListTag)) {
    return fail(error, "Json.Set: the first argument must be a dictionary or a list");
  }
  if (result.number == kListTag) {
    if (!args[1].is_number() || args[1].number < 0 || args[1].number > static_cast<double>(result.fields.size())) {
      return fail(error, "Json.Set: list index out of range");
    }
    const auto index = static_cast<std::size_t>(args[1].number);
    if (index == result.fields.size()) {
      result.fields.push_back(args[2]);
    } else {
      result.fields[index] = args[2];
    }
    out = std::move(result);
    return true;
  }
  const std::string key = key_text(args[1]);
  for (std::size_t index = 0; index + 1 < result.fields.size(); index += 2) {
    if (key_text(result.fields[index]) == key) {
      result.fields[index + 1] = args[2];
      out = std::move(result);
      return true;
    }
  }
  result.fields.push_back(Value::string_of(key));
  result.fields.push_back(args[2]);
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
  for (std::size_t index = 0; index + 1 < args[0].fields.size(); index += 2) {
    keys.push_back(Value::string_of(key_text(args[0].fields[index])));
  }
  out = Value::struct_of(std::move(keys));
  out.number = kListTag;
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kJsonSource = R"clp(<< @clpp.json: JSON text <-> CL++ values.
<< Objects are dictionaries, arrays are lists; true/false become 1/0 and null becomes 0.

func Parse(string text) { return json::Parse(text); }
func Valid(string text) -> bool { return json::Valid(text); }
func Stringify(value) -> string { return json::Stringify(value); }
func Pretty(value) -> string { return json::Stringify(value, 2); }
func Get(value, string path) { return json::Get(value, path); }
func Has(value, string path) -> bool { return json::Has(value, path); }
func Set(target, key, value) { return json::Set(target, key, value); }
func Object() { return json::Object(); }
func Keys(object) { return json::Keys(object); }
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

}  // namespace clpp::stdlib::host
