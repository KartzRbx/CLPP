#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 800; ++i) {
    const std::string source = "let n = " + std::to_string(i) +
                               ";\n"
                               "match (n) {\n"
                               "  0 ~> post(0);\n"
                               "  1 ~> post(1);\n"
                               "  _ guard n > 1 ~> post(2);\n"
                               "}\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    const std::string expected = std::to_string(i == 0 ? 0 : i == 1 ? 1 : 2) + "\n";
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != expected) {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    const std::string source = "match (\"v" + std::to_string(i) + "\") { 1 ~> post(1); }\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (compiled.ok() || !compiled.chunk.code.empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
