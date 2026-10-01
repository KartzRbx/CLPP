#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 1000; ++i) {
    const int a = i % 50;
    const int b = (i / 50) % 20;
    const std::string source = "func add(a, b) {\n  return a + b;\n}\npost(add(" + std::to_string(a) +
                               ", " + std::to_string(b) + "));\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != std::to_string(a + b) + "\n") {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
