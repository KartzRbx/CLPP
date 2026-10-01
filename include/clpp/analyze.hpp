#pragma once

#include "clpp/compiler.hpp"
#include "clpp/core/lexer/token.hpp"
#include "core/parser/ast.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

// One `link` of the analyzed file, kept for the IDE (completion, hover, go to definition).
struct ModuleInfo {
  std::string alias;
  std::string path;
  std::size_t buffer{0};  // index into AnalysisResult::source_buffers
  SourceLocation location{};
  std::vector<std::string> functions;
  std::vector<std::string> types;
  struct Constant {
    std::string name;
    std::string type;     // int, float, bool, string
    std::string display;  // literal as written
    SourceLocation location{};
  };
  std::vector<Constant> constants;  // `const NAME = literal;` / `let NAME = literal;` at module level
};

struct AnalysisResult {
  std::deque<std::string> source_buffers;
  parser::Program program;
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;
  std::vector<ModuleInfo> modules;
  std::vector<parser::LinkDecl> links;  // as written, before import (spans point into the source)
  std::deque<std::string> owned_text;    // storage for composed diagnostic messages

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

[[nodiscard]] AnalysisResult analyze_program(std::string_view source, const ModuleLoader& modules);

}  // namespace clpp
