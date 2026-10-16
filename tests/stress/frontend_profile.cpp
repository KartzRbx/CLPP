// Diagnostic tool for local frontend scaling; no timing assertions in CI.
#include "clpp/core/lexer/lexer.hpp"
#include "core/parser/parser.hpp"
#include "core/binder/binder.hpp"
#include "core/typechecker/solver.hpp"

#include <chrono>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const int count = argc > 1 ? std::stoi(argv[1]) : 10000;
  if (count < 1 || count > 60000) return 2;
  std::string source;
  for (int index = 0; index < count; ++index) {
    source += "const auto value" + std::to_string(index) + " = " + std::to_string(index) + ";\n";
  }
  source += "post(value" + std::to_string(count - 1) + ");\n";
  auto started = std::chrono::steady_clock::now();
  const auto phase = [&](const char* name) {
    const auto now = std::chrono::steady_clock::now();
    std::cout << name << " " << std::chrono::duration<double, std::milli>(now - started).count() << " ms\n" << std::flush;
    started = now;
  };
  const auto lexed = clpp::Lexer(source).tokenize();
  phase("lexer");
  auto parsed = clpp::parser::Parser(lexed.tokens, lexed.diagnostics).parse();
  phase("parser");
  // Match analyze_program: file-scope declarations have shared module slots.
  for (auto& statement : parsed.program.statements) {
    if (statement.kind == clpp::parser::Stmt::Kind::VariableDecl) statement.module_global = true;
  }
  clpp::binder::bind(parsed.program, parsed.diagnostics);
  phase("binder");
  clpp::typechecker::Solver{}.check(parsed.program, parsed.diagnostics);
  phase("typechecker");
  std::cout << "diagnostics " << parsed.diagnostics.size() << '\n';
  return parsed.diagnostics.empty() ? 0 : 1;
}
