#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  const std::filesystem::path dir = "stress-fsprobe";
  std::filesystem::create_directories(dir);
  int failures = 0;
  for (int i = 0; i < 800; ++i) {
    const std::string body(static_cast<std::size_t>(i % 17 + 1), 'a');
    const std::filesystem::path file = dir / ("f" + std::to_string(i) + ".txt");
    {
      std::ofstream out(file, std::ios::binary);
      out << body;
    }
    const std::string path = file.generic_string();
    const std::string source = "link @clpp.fs as Fs;\npost(Fs.size(\"" + path + "\"));\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    std::ostringstream printed;
    clpp::VirtualMachine vm;
    vm.set_output(printed);
    if (!compiled.ok()) {
      ++failures;
      continue;
    }
    vm.load(compiled.chunk);
    if (!vm.run() || printed.str() != std::to_string(body.size()) + "\n") {
      ++failures;
    }
  }
  for (int i = 0; i < 200; ++i) {
    const std::string source = "link @clpp.fs as Fs;\nint x = Fs.size(" + std::to_string(i) + ");\n";
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (compiled.ok() || !compiled.chunk.code.empty()) {
      ++failures;
    }
  }
  std::cout << "ran 1000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
