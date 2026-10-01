#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {

int g_failures = 0;
int g_ran = 0;

void fail(const std::string& source, const std::string& why) {
  if (g_failures < 8) {
    std::cerr << "FAIL " << why << "\n" << source << "\n";
  }
  ++g_failures;
}

void expect_output(const std::string& source, const std::string& expected) {
  ++g_ran;
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
  if (!compiled.ok()) {
    fail(source, "compile");
    return;
  }
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(compiled.chunk);
  if (!vm.run()) {
    fail(source, std::string(vm.error()));
    return;
  }
  if (out.str() != expected) {
    fail(source, "got [" + out.str() + "] want [" + expected + "]");
  }
}

void expect_compile_error(const std::string& source, const std::string& message) {
  ++g_ran;
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
  if (compiled.ok()) {
    fail(source, "expected diagnostic");
    return;
  }
  bool found = false;
  for (const clpp::Diagnostic& diagnostic : compiled.diagnostics) {
    if (diagnostic.message == message) {
      found = true;
      break;
    }
  }
  if (!found) {
    fail(source, "missing diagnostic");
  }
}

void expect_runtime(const std::string& source, const std::string& message) {
  ++g_ran;
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
  if (!compiled.ok()) {
    fail(source, "compile");
    return;
  }
  clpp::VirtualMachine vm;
  vm.load(compiled.chunk);
  if (vm.run() || vm.error() != message) {
    fail(source, "runtime");
  }
}

}  // namespace

int main() {
  for (int i = 0; i < 200000; ++i) {
    const int a = i % 251;
    const int b = (i / 251) % 251;
    const int c = (i / (251 * 251)) % 17;
    const int value = (a + b) * (c + 1);
    expect_output("let a = " + std::to_string(a) + ";\nlet b = " + std::to_string(b) +
                      ";\nlet c = " + std::to_string(c) + ";\npost((a + b) * (c + 1));\n",
                  std::to_string(value) + "\n");
  }

  for (int i = 0; i < 100000; ++i) {
    const int value = i % 10000;
    expect_output("let x = " + std::to_string(value) + ";\npost(x);\n", std::to_string(value) + "\n");
  }

  for (int i = 0; i < 100000; ++i) {
    const int value = i % 2;
    expect_output("if (" + std::to_string(value) + ") { post(1); } else { post(0); }\n",
                  value == 0 ? "0\n" : "1\n");
  }

  for (int i = 0; i < 50000; ++i) {
    const int limit = (i % 15) + 1;
    const int sum = limit * (limit + 1) / 2;
    expect_output("let mut total = 0;\nlet mut i = 1;\nwhile (i <= " + std::to_string(limit) +
                      ") {\n  total = total + i;\n  i = i + 1;\n}\npost(total);\n",
                  std::to_string(sum) + "\n");
  }

  for (int i = 0; i < 25000; ++i) {
    expect_compile_error("post(missing" + std::to_string(i % 50) + ");\n", "undefined name");
  }

  for (int i = 0; i < 25000; ++i) {
    const int numerator = (i % 40) + 1;
    expect_runtime("post(" + std::to_string(numerator) + " / 0);\n", "division by zero");
  }

  std::cout << "ran " << g_ran << " failures " << g_failures << "\n";
  return g_failures == 0 && g_ran == 500000 ? 0 : 1;
}
