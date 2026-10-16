#include "clpp/detail/utf8_path.hpp"
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
#include <shellapi.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // Decode Unicode arguments independently of the CRT's legacy narrow code page.
  int wide_count = 0;
  wchar_t** wide_args = CommandLineToArgvW(GetCommandLineW(), &wide_count);
  if (!wide_args) { std::cerr << "cannot decode command line\n"; return 1; }
  std::vector<std::string> utf8_args;
  utf8_args.reserve(static_cast<std::size_t>(wide_count));
  for (int index = 0; index < wide_count; ++index) {
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_args[index], -1, nullptr, 0, nullptr, nullptr);
    if (length == 0) { LocalFree(wide_args); std::cerr << "invalid Unicode command line\n"; return 1; }
    std::string text(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_args[index], -1, text.data(), length, nullptr, nullptr);
    text.pop_back(); utf8_args.push_back(std::move(text));
  }
  LocalFree(wide_args);
  std::vector<char*> utf8_pointers;
  for (auto& text : utf8_args) utf8_pointers.push_back(text.data());
  utf8_pointers.push_back(nullptr);
  argc = wide_count; argv = utf8_pointers.data();
  // Programs print UTF-8 ("ação", "naïve"). Without this the Windows console shows it in the OEM
  // code page ("a├º├úo"). Pipes and files are unaffected: they always get the raw UTF-8 bytes.
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
  const auto print_help = [](std::ostream& out) {
    out << "Usage: clpp <script.clp> [args...]\n";
    out << "       clpp-vm --check <script.clp>\n";
    out << "       clpp-vm --repl\n";
    out << "       clpp-vm --lsp\n";
    out << "       clpp-vm --help\n";
    out << "       clpp-vm --version\n";
  };
  if (argc < 2) {
    print_help(std::cerr);
    return 1;
  }

  const bool check_only = std::string_view(argv[1]) == "--check";
  if (check_only && argc < 3) { print_help(std::cerr); return 1; }
  const std::string_view arg = argv[check_only ? 2 : 1];
  if (arg == "--help") {
    print_help(std::cout);
    return 0;
  }
  if (arg == "--version") {
    std::cout << "clpp-vm " << CLPP_VERSION << '\n';
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

  std::ifstream file(clpp::detail::utf8_path(arg), std::ios::binary);
  if (!file) {
    std::cerr << "cannot open " << arg << '\n';
    return 1;
  }

  const std::string source(std::istreambuf_iterator<char>(file), {});
  const std::filesystem::path base = clpp::detail::utf8_path(arg).parent_path();
  const clpp::ModuleLoader modules = [&base](const std::string_view path) -> std::optional<std::string> {
    std::ifstream module(base / clpp::detail::utf8_path(path), std::ios::binary);
    if (!module) {
      return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(module), {});
  };
  const clpp::CompileResult result = clpp::Compiler{}.compile(source, modules);
  if (!result.ok()) {
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      std::cerr << std::string(arg) << ':' << diagnostic.location.line << ':' << diagnostic.location.column
                << ": error: " << diagnostic.formatted_message() << '\n';
    }
    return 1;
  }
  if (check_only) return 0;

  clpp::VirtualMachine vm;
  vm.set_output(std::cout);
  vm.load(result.chunk);
  if (!vm.run()) {
    const clpp::SourceLocation where = vm.error_location();
    if (where.line != 0) {
      std::string origin(arg);
      if (where.source < result.chunk.source_files.size()) {
        const auto& path = result.chunk.source_files[where.source];
        if (!path.empty()) {
          if (path.front() == '@') origin = path;
          else {
            const auto encoded = (base / clpp::detail::utf8_path(path)).lexically_normal().u8string();
            origin.assign(encoded.begin(), encoded.end());
          }
        }
      }
      std::cerr << origin << ':' << where.line << ':' << where.column << ": ";
    }
    std::cerr << "runtime error: " << vm.error() << '\n';
    return 1;
  }
  // Flush script output before global worker slots are destroyed at process shutdown.
  std::cout.flush();
  return clpp::stdlib::program_exit_code();
}
