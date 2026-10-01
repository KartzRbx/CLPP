#include "clpp/repl.hpp"

#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace clpp {

namespace {

[[nodiscard]] std::string trim(const std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

}  // namespace

int run_repl(std::istream& in, std::ostream& out, std::ostream& err) {
  std::string line;
  while (true) {
    err << "clpp> ";
    if (!std::getline(in, line)) {
      return 0;
    }
    const std::string source = trim(line);
    if (source.empty()) {
      continue;
    }
    if (source == "exit" || source == "quit") {
      return 0;
    }

    const CompileResult compiled = Compiler{}.compile(source);
    if (!compiled.ok()) {
      for (const Diagnostic& diagnostic : compiled.diagnostics) {
        err << diagnostic.location.line << ':' << diagnostic.location.column << ": " << diagnostic.message << '\n';
      }
      continue;
    }

    VirtualMachine vm;
    vm.set_output(out);
    vm.load(compiled.chunk);
    if (!vm.run()) {
      err << "runtime error: " << vm.error() << '\n';
    }
  }
}

}  // namespace clpp
