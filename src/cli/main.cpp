#include "clpp/clpp.hpp"
#include "clpp/ide.hpp"
#include "clpp/repl.hpp"
#include "clpp/stdlib.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // Programs print UTF-8 ("ação", "naïve"). Without this the Windows console shows it in the OEM
  // code page ("a├º├úo"). Pipes and files are unaffected: they always get the raw UTF-8 bytes.
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
  const auto print_help = [](std::ostream& out) {
    out << "Usage: clpp <script.clp> [args...]\n";
    out << "       clpp --repl\n";
    out << "       clpp --lsp\n";
    out << "       clpp --help\n";
    out << "       clpp --version\n";
  };
  if (argc < 2) {
    print_help(std::cerr);
    return 1;
  }

  const std::string_view arg = argv[1];
  if (arg == "--help") {
    print_help(std::cout);
    return 0;
  }
  if (arg == "--version") {
    std::cout << "clpp 0.10\n";
    return 0;
  }
  if (arg == "--repl") {
    return clpp::run_repl(std::cin, std::cout, std::cerr);
  }
  if (arg == "--lsp") {
    return clpp::ide::run_lsp(std::cin, std::cout);
  }

  std::vector<std::string> program_args;
  for (int index = 2; index < argc; ++index) {
    program_args.emplace_back(argv[index]);
  }
  clpp::stdlib::set_program_args(program_args);

  std::ifstream file(std::string(arg), std::ios::binary);
  if (!file) {
    std::cerr << "cannot open " << arg << '\n';
    return 1;
  }

  const std::string source(std::istreambuf_iterator<char>(file), {});
  const std::filesystem::path base = std::filesystem::path(std::string(arg)).parent_path();
  const clpp::ModuleLoader modules = [&base](const std::string_view path) -> std::optional<std::string> {
    std::ifstream module(base / std::string(path), std::ios::binary);
    if (!module) {
      return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(module), {});
  };
  const clpp::CompileResult result = clpp::Compiler{}.compile(source, modules);
  if (!result.ok()) {
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      std::cerr << std::string(arg) << ':' << diagnostic.location.line << ':' << diagnostic.location.column
                << ": error: " << diagnostic.message << '\n';
    }
    return 1;
  }

  clpp::VirtualMachine vm;
  vm.set_output(std::cout);
  vm.load(result.chunk);
  if (!vm.run()) {
    const clpp::SourceLocation where = vm.error_location();
    if (where.line != 0) {
      std::cerr << std::string(arg) << ':' << where.line << ':' << where.column << ": ";
    }
    std::cerr << "runtime error: " << vm.error() << '\n';
    return 1;
  }
  return 0;
}
