#include "clpp/stdlib.hpp"

namespace clpp::stdlib {

const std::vector<ModuleEntry>& module_catalog() {
  static const std::vector<ModuleEntry> catalog = {
      {"@clpp.math", "Math", "Basic math: abs"},
      {"@clpp.axiom", "Axiom", "Game math: Clamp, Lerp, Smoothstep, vectors, angles, easing"},
      {"@clpp.text", "Text", "Text: trim, lower, upper, contains, split, replace, startsWith, endsWith, repeat, toNumber, indexOf, joinAll"},
      {"@clpp.fs", "Fs", "Files: size, read, list"},
      {"@clpp.os", "Os", "Process: env"},
      {"@clpp.http", "Http", "URLs: host"},
  };
  return catalog;
}

std::optional<std::string> module_source(const std::string_view path) {
  if (path == "@clpp.axiom" || path == "@clpp.Axion" || path == "@clpp.libs.axiom" || path == "@clpp.libs.Axion" ||
      path == "@clpp.mathutils") {
    return std::string(axiom_source());
  }
  if (path == "@clpp.math") {
    return std::string(
        "func abs(n) {\n"
        "  if (n < 0) {\n"
        "    return 0 - n;\n"
        "  }\n"
        "  return n;\n"
        "}\n");
  }
  if (path == "@clpp.fs") {
    return std::string(
        "func size(string path) {\n"
        "  return fs::size(path);\n"
        "}\n"
        "func read(string path) {\n"
        "  return fs::read(path);\n"
        "}\n"
        "func list(string path) {\n"
        "  return fs::list(path);\n"
        "}\n");
  }
  if (path == "@clpp.text") {
    return std::string(
        "func trim(string text) -> string { return string::trim(text); }\n"
        "func lower(string text) -> string { return string::lower(text); }\n"
        "func upper(string text) -> string { return string::upper(text); }\n"
        "func contains(string text, string part) -> bool { return string::contains(text, part); }\n"
        "func split(string text, string separator) { return string::split(text, separator); }\n"
        "func replace(string text, string target, string replacement) -> string { return string::replace(text, target, replacement); }\n"
        "func startsWith(string text, string part) -> bool { return string::starts_with(text, part); }\n"
        "func endsWith(string text, string part) -> bool { return string::ends_with(text, part); }\n"
        "func repeat(string text, int count) -> string { return string::repeat(text, count); }\n"
        "func toNumber(string text) -> float { return string::to_number(text); }\n"
        "func indexOf(string text, string part) -> int { return string::index_of(text, part); }\n"
        "func joinAll(parts, string separator) -> string { return string::join_list(parts, separator); }\n");
  }
  if (path == "@clpp.os") {
    return std::string(
        "func env(string name) {\n"
        "  return os::env(name);\n"
        "}\n");
  }
  if (path == "@clpp.http") {
    return std::string(
        "func host(string url) {\n"
        "  return http::host(url);\n"
        "}\n");
  }
  return std::nullopt;
}

}  // namespace clpp::stdlib
