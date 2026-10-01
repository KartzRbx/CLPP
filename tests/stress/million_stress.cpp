#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

const clpp::ModuleLoader kModules = [](const std::string_view) -> std::optional<std::string> {
  return std::nullopt;
};

[[nodiscard]] bool expect_output(const std::string& source, const std::string& expected) {
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, kModules);
  if (!compiled.ok()) {
    return false;
  }
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(compiled.chunk);
  return vm.run() && out.str() == expected;
}

[[nodiscard]] bool expect_fail(const std::string& source) {
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, kModules);
  return !compiled.ok() && compiled.chunk.code.empty();
}

[[nodiscard]] std::string tag(const int i) {
  return "<< sim " + std::to_string(i) + "\n";
}

}  // namespace

int main() {
  int failures = 0;
  int reported = 0;
  for (int i = 0; i < 1000000; ++i) {
    const int bucket = i % 20;
    const int n = i % 97;
    const std::string mark = tag(i);
    bool ok = false;
    switch (bucket) {
      case 0:
        ok = expect_output(mark + "post(" + std::to_string(n) + " + 2 * 3);\n", std::to_string(n + 6) + "\n");
        break;
      case 1:
        ok = expect_output(mark + "let mut x = " + std::to_string(n) + ";\nx = x + 1;\npost(x);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 2: {
        const int limit = n % 5;
        const int sum = limit * (limit - 1) / 2;
        ok = expect_output(mark + "let mut s = 0;\nfor (let i in " + std::to_string(limit) +
                               ") { s = s + i; }\npost(s);\n",
                           std::to_string(sum) + "\n");
        break;
      }
      case 3:
        ok = expect_output(mark + "constexpr x = " + std::to_string(n) + " + 2;\npost(x);\n",
                           std::to_string(n + 2) + "\n");
        break;
      case 4:
        ok = expect_fail(mark + "constexpr x = name;\n");
        break;
      case 5:
        ok = expect_output(mark + "post(Vector2(" + std::to_string(n) + ", " + std::to_string(n + 1) + ")[1]);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 6:
        ok = expect_output(mark + "post(Vector4(1, 2, 3, " + std::to_string(n) + ").w);\n", std::to_string(n) + "\n");
        break;
      case 7:
        ok = expect_output(mark + "post(\"AZ\"[" + std::to_string(n % 2) + "]);\n",
                           std::string(n % 2 == 0 ? "65\n" : "90\n"));
        break;
      case 8:
        ok = expect_output(mark + "post(pcall(1 / " + std::to_string(n % 2) + "));\n",
                           std::string(n % 2 == 0 ? "0\n" : "1\n"));
        break;
      case 9:
        ok = expect_output(mark + "cout(" + std::to_string(n) + ");\n", std::to_string(n) + "\n");
        break;
      case 10:
        ok = expect_output(mark + "auto x = " + std::to_string(n) + ";\npost(x);\n", std::to_string(n) + "\n");
        break;
      case 11:
        ok = expect_output(mark + "observable x = " + std::to_string(n) + ";\nx = x + 1;\npost(x);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 12:
        ok = expect_output(mark + "enum Color { Red, Green }\nlet color = " +
                               std::string(n % 2 == 0 ? "Color.Red" : "Color.Green") +
                               ";\nmatch (color) { Red ~> post(0); Green ~> post(1); }\n",
                           std::string(n % 2 == 0 ? "0\n" : "1\n"));
        break;
      case 13:
        ok = expect_fail(mark + "let x = " + std::to_string(n) + ";\nx = 1;\n");
        break;
      case 14:
        ok = expect_output(mark + "signal ping;\nfunc show(int x) { post(x); }\nping ~> show;\nping(" +
                               std::to_string(n) + ");\n",
                           std::to_string(n) + "\n");
        break;
      case 15:
        ok = expect_output(mark + "struct Point { int x; int y; }\npost(new Point(" + std::to_string(n) +
                               ", 1).x);\n",
                           std::to_string(n) + "\n");
        break;
      case 16:
        ok = expect_output(mark + "@flag = " + std::to_string(n) + ";\npost(@this::flag);\n", std::to_string(n) + "\n");
        break;
      case 17: {
        const int extra = n % 3;
        ok = expect_output(mark + "let mut total = 0;\nfor (let mut k = 0; k < 3; k += 1) { total = total + k; }\npost(total + " +
                               std::to_string(extra) + ");\n",
                           std::to_string(3 + extra) + "\n");
        break;
      }
      case 18:
        ok = expect_output(mark + "import @clpp.math as Math;\nusing Math.abs;\npost(abs(0 - " + std::to_string(n + 1) +
                               "));\n",
                           std::to_string(n + 1) + "\n");
        break;
      default:
        ok = expect_output(mark + "warn(" + std::to_string(n) + ");\nreport(" + std::to_string(n) + ");\n",
                           std::to_string(n) + "\n" + std::to_string(n) + "\n");
        break;
    }
    if (!ok) {
      ++failures;
      if (reported < 8) {
        std::cout << "FAIL bucket " << bucket << " i " << i << " n " << n << '\n';
        ++reported;
      }
    }
  }
  std::cout << "ran 1000000 failures " << failures << '\n';
  return failures == 0 ? 0 : 1;
}
