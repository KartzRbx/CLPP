#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

int main() {
  int failures = 0;
  const auto modules = [](const std::string_view path) -> std::optional<std::string> {
    if (path == "./math.clp") {
      return std::string("func add(a, b) { return a + b; }\n");
    }
    return std::nullopt;
  };
  for (int i = 0; i < 800; ++i) {
    const std::string source = "link \"./math.clp\" as Math;\npost(Math.add(" + std::to_string(i) + ", 1));\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, modules);
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
    const std::string source = "link \"./missing" + std::to_string(i) + ".clp\" as Math;\npost(Math.add(1, 1));\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, modules);
    if (compiled.ok() || !compiled.chunk.code.empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
