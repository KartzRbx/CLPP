#pragma once

#include "clpp/core/lexer/token.hpp"
#include "core/parser/ast.hpp"

#include <vector>

namespace clpp::typechecker {

class Solver {
 public:
  void check(const parser::Program& program, std::vector<Diagnostic>& diagnostics);
};

}  // namespace clpp::typechecker
