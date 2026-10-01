#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 800; ++i) {
    const std::string source = "let n = " + std::to_string(i) + ";\npost(`n=${n}`);\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != "n=" + std::to_string(i) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    const std::string source = "int x = `v${" + std::to_string(i) + "}`;\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (compiled.ok() || !compiled.chunk.code.empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
