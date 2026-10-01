#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 800; ++i) {
    const std::string source = "async func add(a, b) { return a + b; }\n"
                               "task job = spawn add(" +
                               std::to_string(i) +
                               ", 1);\n"
                               "post(await job);\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != std::to_string(i + 1) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    const std::string source = "async func add(a, b) { return a + b; }\nint x = spawn add(" + std::to_string(i) +
                               ", 1);\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (compiled.ok() || !compiled.chunk.code.empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
