#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
  int failures = 0;
  for (int i = 0; i < 1000; ++i) {
    const int x = i % 10;
    const int y = (i / 10) % 10;
    const int z = (i / 100) % 10;
    const int scale = (i % 5) + 1;
    const std::string source = "let v = Vector3(" + std::to_string(x) + ", " + std::to_string(y) + ", " +
                               std::to_string(z) + ");\nlet w = v * " + std::to_string(scale) +
                               ";\npost(w.x);\npost(w.y);\npost(w.z);\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    const std::string expected = std::to_string(x * scale) + "\n" + std::to_string(y * scale) + "\n" +
                                 std::to_string(z * scale) + "\n";
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    std::ostringstream out;
    clpp::VirtualMachine vm;
    vm.set_output(out);
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != expected) {
      if (failures < 3) {
        std::cerr << out.str() << " want " << expected;
      }
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
