#include "clpp/compiler.hpp"
#include "clpp/repl.hpp"
#include "clpp/vm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

const clpp::ModuleLoader kModules = [](const std::string_view path) -> std::optional<std::string> {
  if (path == "./m.clp") {
    return std::string("func add(a, b) { return a + b; }\n");
  }
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

[[nodiscard]] bool expect_div0(const std::string& source) {
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, kModules);
  if (!compiled.ok()) {
    return false;
  }
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(compiled.chunk);
  return !vm.run() && vm.error() == "division by zero";
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
  const std::filesystem::path probe = "stress-fsprobe/fixed.txt";
  std::filesystem::create_directories(probe.parent_path());
  {
    std::ofstream file(probe, std::ios::binary);
    file << "abcdefg";
  }

  int failures = 0;
  int reported = 0;
  for (int i = 0; i < 500000; ++i) {
    const int bucket = i % 25;
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
      case 2:
        ok = expect_output(mark + "let mut total = 0;\nlet mut k = 1;\nwhile (k <= 4) {\n  total = total + k;\n  k = k + 1;\n}\npost(total + " +
                               std::to_string(n % 3) + ");\n",
                           std::to_string(10 + (n % 3)) + "\n");
        break;
      case 3:
        ok = expect_output(mark + "if (" + std::to_string(n % 2) + " == 0) {\n  post(1);\n} else {\n  post(0);\n}\n",
                           std::string(n % 2 == 0 ? "1\n" : "0\n"));
        break;
      case 4:
        if (n % 5 == 0) {
          ok = expect_output(mark + "post(0 and (1 / 0));\n", "0\n");
        } else {
          ok = expect_output(mark + "post(" + std::to_string(n % 2) + " and 1);\n",
                             std::string(n % 2 == 0 ? "0\n" : "1\n"));
        }
        break;
      case 5:
        ok = expect_output(mark + "func add(a, b) { return a + b; }\npost(add(" + std::to_string(n) + ", 1));\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 6:
        ok = expect_output(mark + "Vector3 v = Vector3(" + std::to_string(n) + ", 2, 3);\npost(v.x);\n",
                           std::to_string(n) + "\n");
        break;
      case 7:
        ok = expect_output(mark + "int x = " + std::to_string(n) + ";\npost(x);\n", std::to_string(n) + "\n");
        break;
      case 8:
        ok = expect_output(mark + "struct Point { int x; int y; }\nPoint p = Point(" + std::to_string(n) +
                               ", 1);\npost(p.x + p.y);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 9:
        ok = expect_output(mark + "enum Color { Red, Green, Blue }\nlet n = " + std::to_string(n % 3) +
                               ";\nmatch (n) {\n  0 ~> post(Color.Red);\n  1 ~> post(Color.Green);\n  _ ~> post(Color.Blue);\n}\n",
                           std::to_string(n % 3) + "\n");
        break;
      case 10:
        ok = expect_output(mark + "let n = " + std::to_string(n) + ";\npost(`n=${n}`);\n", "n=" + std::to_string(n) + "\n");
        break;
      case 11:
        ok = expect_output(mark + "@coins = " + std::to_string(n) + ";\n@this.coins = @coins + 1;\npost(@coins);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 12:
        ok = expect_output(mark + "link \"./m.clp\" as Math;\npost(Math.add(" + std::to_string(n) + ", 1));\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 13:
        ok = expect_output(mark + "let n = " + std::to_string(n % 3) +
                               ";\nmatch (n) {\n  0 ~> post(10);\n  1 ~> post(20);\n  _ guard n > 1 ~> post(30);\n}\n",
                           std::string(n % 3 == 0 ? "10\n" : n % 3 == 1 ? "20\n" : "30\n"));
        break;
      case 14:
        ok = expect_output(mark + "async func add(a, b) { return a + b; }\npost(await add(" + std::to_string(n) +
                               ", 1));\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 15:
        ok = expect_output(mark + "async func add(a, b) { return a + b; }\ntask job = spawn add(" + std::to_string(n) +
                               ", 1);\npost(await job);\n",
                           std::to_string(n + 1) + "\n");
        break;
      case 16:
        ok = expect_output(mark + "async func add(a, b) { return a + b; }\npost(parallel(add(" + std::to_string(n) +
                               ", 1), add(" + std::to_string(n) + ", 2)));\n",
                           std::to_string((n + 1) + (n + 2)) + "\n");
        break;
      case 17:
        ok = expect_output(mark + "link @clpp.math as Math;\npost(Math.abs(0 - " + std::to_string(n) + "));\n",
                           std::to_string(n) + "\n");
        break;
      case 18:
        if (n % 2 == 0) {
          ok = expect_output(mark + "link @clpp.fs as Fs;\npost(Fs.size(\"stress-fsprobe/fixed.txt\"));\n", "7\n");
        } else {
          ok = expect_output(mark + "link @clpp.fs as Fs;\npost(Fs.size(\"stress-fsprobe/missing-" +
                                 std::to_string(i) + "\"));\n",
                             "-1\n");
        }
        break;
      case 19: {
        std::istringstream in(mark + "post(" + std::to_string(n) + ");\nexit\n");
        std::ostringstream out;
        std::ostringstream err;
        ok = clpp::run_repl(in, out, err) == 0 && out.str() == std::to_string(n) + "\n";
        break;
      }
      case 20:
        ok = expect_output(mark + "buffer box = buffer::create(" + std::to_string(n) + ");\npost(buffer::size(box));\n",
                           std::to_string(n) + "\n");
        break;
      case 21:
        ok = expect_output(mark + "post(\"n\" .: " + std::to_string(n % 10) + ");\n", "n" + std::to_string(n % 10) + "\n");
        break;
      case 22:
        ok = expect_output(mark + "post(not " + std::to_string(n % 2) + ");\n", std::string(n % 2 == 0 ? "1\n" : "0\n"));
        break;
      case 23:
        ok = expect_div0(mark + "post(1 / 0);\n");
        break;
      case 24: {
        const int kind = i % 5;
        if (kind == 0) {
          ok = expect_fail(mark + "int x = \"v" + std::to_string(i) + "\";\n");
        } else if (kind == 1) {
          ok = expect_fail(mark + "post(missing" + std::to_string(i) + ");\n");
        } else if (kind == 2) {
          ok = expect_fail(mark + "func id(a) { return a; }\npost(id());\n");
        } else if (kind == 3) {
          ok = expect_fail(mark + "match (\"z" + std::to_string(i) + "\") { 1 ~> post(1); }\n");
        } else {
          ok = expect_fail(mark + "link \"./missing" + std::to_string(i) + ".clp\" as M;\npost(1);\n");
        }
        break;
      }
      default:
        ok = false;
        break;
    }
    if (!ok) {
      ++failures;
      if (reported < 5) {
        std::cout << "fail i " << i << " bucket " << bucket << "\n";
        ++reported;
      }
    }
    if ((i + 1) % 50000 == 0) {
      std::cout << "progress " << (i + 1) << " failures " << failures << "\n";
    }
  }
  std::cout << "ran 500000 failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
