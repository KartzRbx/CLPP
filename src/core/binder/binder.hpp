#pragma once

#include "clpp/core/lexer/token.hpp"
#include "core/parser/ast.hpp"

#include <vector>

namespace clpp::binder {

void bind(parser::Program& program, std::vector<Diagnostic>& diagnostics);

}  // namespace clpp::binder
