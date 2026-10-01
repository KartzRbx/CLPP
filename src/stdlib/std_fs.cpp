#include "clpp/stdlib.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace clpp::stdlib {

double file_size(const std::string_view path) {
  std::ifstream file(std::string(path), std::ios::binary | std::ios::ate);
  if (!file) {
    return -1;
  }
  const std::streamoff size = file.tellg();
  if (size < 0) {
    return -1;
  }
  return static_cast<double>(size);
}

namespace {

std::vector<std::string> g_args;

[[nodiscard]] std::string trimmed(std::string text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

[[nodiscard]] std::string format_plain(const double number) {
  if (number == static_cast<double>(static_cast<long long>(number)) && number < 1e15 && number > -1e15) {
    return std::to_string(static_cast<long long>(number));
  }
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.17g", number);
  return buffer;
}

[[nodiscard]] bool bad(std::string& error) {
  error = "type error";
  return false;
}

}  // namespace

void set_program_args(const std::vector<std::string>& args) { g_args = args; }

bool std_apply(const std::uint8_t id, const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  if (id == 6) {
    std::vector<Value> fields;
    fields.reserve(g_args.size());
    for (const std::string& arg : g_args) {
      fields.push_back(Value::string_of(arg));
    }
    out = Value::struct_of(std::move(fields));
    return true;
  }
  if (id == 14) {  // join(list, separator)
    if (args == nullptr || arity != 2 || !args[0].is_struct() || !args[1].is_string()) {
      return bad(error);
    }
    std::string joined;
    for (std::size_t index = 0; index < args[0].fields.size(); ++index) {
      if (index != 0) {
        joined += args[1].text;
      }
      const Value& part = args[0].fields[index];
      joined += part.is_string() ? part.text : part.is_number() ? format_plain(part.number) : std::string{};
    }
    out = Value::string_of(std::move(joined));
    return true;
  }
  if (args == nullptr || arity < 1 || !args[0].is_string()) {
    return bad(error);
  }
  const std::string& text = args[0].text;
  if (id == 7) {  // split(text, separator); an empty separator splits into characters
    if (arity != 2 || !args[1].is_string()) {
      return bad(error);
    }
    std::vector<Value> parts;
    const std::string& separator = args[1].text;
    if (separator.empty()) {
      for (std::size_t offset = 0; offset < text.size();) {
        const auto lead = static_cast<unsigned char>(text[offset]);
        const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        parts.push_back(Value::string_of(text.substr(offset, width)));
        offset += width;
      }
    } else {
      std::size_t start = 0;
      while (true) {
        const std::size_t found = text.find(separator, start);
        parts.push_back(Value::string_of(text.substr(start, found == std::string::npos ? std::string::npos : found - start)));
        if (found == std::string::npos) {
          break;
        }
        start = found + separator.size();
      }
    }
    out = Value::struct_of(std::move(parts));
    return true;
  }
  if (id == 8) {  // replace(text, from, to): every occurrence
    if (arity != 3 || !args[1].is_string() || !args[2].is_string() || args[1].text.empty()) {
      return bad(error);
    }
    std::string result;
    std::size_t start = 0;
    while (true) {
      const std::size_t found = text.find(args[1].text, start);
      if (found == std::string::npos) {
        result += text.substr(start);
        break;
      }
      result += text.substr(start, found - start);
      result += args[2].text;
      start = found + args[1].text.size();
    }
    out = Value::string_of(std::move(result));
    return true;
  }
  if (id == 9 || id == 10) {  // starts_with / ends_with
    if (arity != 2 || !args[1].is_string()) {
      return bad(error);
    }
    const std::string& part = args[1].text;
    const bool hit = part.size() <= text.size() &&
                     (id == 9 ? text.compare(0, part.size(), part) == 0
                              : text.compare(text.size() - part.size(), part.size(), part) == 0);
    out = Value::number_of(hit ? 1 : 0);
    return true;
  }
  if (id == 11) {  // repeat(text, count)
    if (arity != 2 || !args[1].is_number() || args[1].number < 0 || args[1].number > 1000000) {
      return bad(error);
    }
    std::string result;
    for (int index = 0; index < static_cast<int>(args[1].number); ++index) {
      result += text;
    }
    out = Value::string_of(std::move(result));
    return true;
  }
  if (id == 12) {  // to_number(text)
    const std::string clean = trimmed(text);
    char* end = nullptr;
    const double number = std::strtod(clean.c_str(), &end);
    if (clean.empty() || end == nullptr || *end != '\0') {
      error = "not a number: \"" + text + "\"";
      return false;
    }
    out = Value::number_of(number);
    return true;
  }
  if (id == 13) {  // index_of(text, part): position in characters, or -1
    if (arity != 2 || !args[1].is_string()) {
      return bad(error);
    }
    const std::size_t found = text.find(args[1].text);
    if (found == std::string::npos) {
      out = Value::number_of(-1);
      return true;
    }
    double characters = 0;
    for (std::size_t offset = 0; offset < found; ++offset) {
      if ((static_cast<unsigned char>(text[offset]) & 0xC0) != 0x80) {
        characters += 1;
      }
    }
    out = Value::number_of(characters);
    return true;
  }
  if (id == 0) {
    out = Value::string_of(trimmed(text));
    return true;
  }
  if (id == 1 || id == 2) {
    std::string changed = text;
    for (char& character : changed) {
      const auto byte = static_cast<unsigned char>(character);
      character = static_cast<char>(id == 1 ? std::tolower(byte) : std::toupper(byte));
    }
    out = Value::string_of(std::move(changed));
    return true;
  }
  if (id == 3) {
    if (arity != 2 || !args[1].is_string()) {
      return bad(error);
    }
    out = Value::number_of(text.find(args[1].text) == std::string::npos ? 0 : 1);
    return true;
  }
  if (id == 4) {
    if (text.find("..") != std::string::npos) {
      error = "runtime error";
      return false;
    }
    std::ifstream file(text, std::ios::binary);
    if (!file) {
      out = Value::string_of("");
      return true;
    }
    out = Value::string_of(std::string(std::istreambuf_iterator<char>(file), {}));
    return true;
  }
  if (id == 5) {
    if (text.find("..") != std::string::npos) {
      error = "runtime error";
      return false;
    }
    std::vector<Value> names;
    std::error_code failure;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(text, failure)) {
      if (failure) {
        break;
      }
      names.push_back(Value::string_of(entry.path().filename().string()));
    }
    out = Value::struct_of(std::move(names));
    return true;
  }
  return bad(error);
}

}  // namespace clpp::stdlib
