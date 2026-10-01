#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

namespace {

// _putenv_s only exists on Windows; setenv only on POSIX.
void set_test_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

}  // namespace

namespace {

[[nodiscard]] bool expect_output(const std::string& source, const std::string& expected) {
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
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
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
  return !compiled.ok() && compiled.chunk.code.empty();
}

int run_feature(const char* name, const std::function<bool(int)>& check) {
  int failures = 0;
  int reported = 0;
  for (int i = 0; i < 500000; ++i) {
    if (!check(i)) {
      ++failures;
      if (reported < 3) {
        std::cout << "fail " << name << " i " << i << "\n";
        ++reported;
      }
    }
    if ((i + 1) % 100000 == 0) {
      std::cout << name << " " << (i + 1) << " failures " << failures << "\n";
    }
  }
  std::cout << name << " ran 500000 failures " << failures << "\n";
  return failures;
}

}  // namespace

int main() {
  int failures = 0;

  failures += run_feature("unary", [](const int i) {
    const int n = i % 97;
    if (i % 2 == 0) {
      return expect_output("let x = " + std::to_string(n + 1) + ";\npost(-x);\n", "-" + std::to_string(n + 1) + "\n");
    }
    return expect_output("post(!" + std::to_string(n % 2) + ");\n", std::string(n % 2 == 0 ? "1\n" : "0\n"));
  });

  failures += run_feature("compound", [](const int i) {
    const int base = i % 20;
    const int op = i % 5;
    if (op == 0) {
      return expect_output("let mut x = " + std::to_string(base) + ";\nx += 3;\npost(x);\n", std::to_string(base + 3) + "\n");
    }
    if (op == 1) {
      return expect_output("let mut x = " + std::to_string(base + 5) + ";\nx -= 2;\npost(x);\n",
                           std::to_string(base + 3) + "\n");
    }
    if (op == 2) {
      return expect_output("let mut x = " + std::to_string(base) + ";\nx *= 2;\npost(x);\n", std::to_string(base * 2) + "\n");
    }
    if (op == 3) {
      return expect_output("let mut x = " + std::to_string(base * 2) + ";\nx /= 2;\npost(x);\n", std::to_string(base) + "\n");
    }
    return expect_output("let mut x = " + std::to_string(base + 5) + ";\nx %= 5;\npost(x);\n",
                         std::to_string((base + 5) % 5) + "\n");
  });

  failures += run_feature("bool_null", [](const int i) {
    const int n = i % 97;
    const int kind = i % 3;
    if (kind == 0) {
      return expect_output("if (true) { post(" + std::to_string(n) + "); } else { post(0); }\n", std::to_string(n) + "\n");
    }
    if (kind == 1) {
      return expect_output("if (false) { post(0); } else { post(" + std::to_string(n) + "); }\n",
                           std::to_string(n) + "\n");
    }
    return expect_output("if (null) { post(0); } else { post(" + std::to_string(n) + "); }\n", std::to_string(n) + "\n");
  });

  failures += run_feature("for", [](const int i) {
    const int extra = i % 3;
    return expect_output(
        "let mut total = 0;\nfor (let mut k = 0; k < 4; k += 1) { total = total + k; }\npost(total + " +
            std::to_string(extra) + ");\n",
        std::to_string(6 + extra) + "\n");
  });

  failures += run_feature("break", [](const int i) {
    const int stop = i % 7;
    return expect_output(
        "let mut seen = 0;\nlet mut k = 0;\nwhile (k < 20) {\n  if (k == " + std::to_string(stop) +
            ") { break; }\n  seen = seen + 1;\n  k = k + 1;\n}\npost(seen);\n",
        std::to_string(stop) + "\n");
  });

  failures += run_feature("switch", [](const int i) {
    const int arm = i % 4;
    const int expected = arm == 0 ? 10 : arm == 1 ? 20 : arm == 2 ? 30 : 40;
    return expect_output(
        "let m = " + std::to_string(arm) +
            ";\nswitch (m) {\n  case 0: post(10); break; post(99);\n  case 1: post(20); break; post(99);\n"
            "  case 2: post(30); break; post(99);\n  default: post(40); break; post(99);\n}\n",
        std::to_string(expected) + "\n");
  });

  failures += run_feature("const", [](const int i) {
    if (i % 5 == 0) {
      return expect_fail("const x = 1;\nx = " + std::to_string(i) + ";\n");
    }
    const int n = i % 97;
    return expect_output("const x = " + std::to_string(n) + ";\npost(x);\n", std::to_string(n) + "\n");
  });

  failures += run_feature("os", [](const int i) {
    const std::string value = std::to_string(i % 97);
    set_test_env("CLPP_SIM", value.c_str());
    return expect_output("link @clpp.os as Os;\npost(Os.env(\"CLPP_SIM\"));\n", value + "\n");
  });

  failures += run_feature("http", [](const int i) {
    const std::string host = "h" + std::to_string(i % 97) + ".test";
    return expect_output("link @clpp.http as Http;\npost(Http.host(\"http://" + host + "/a\"));\n", host + "\n");
  });

  std::cout << "all features failures " << failures << "\n";
  return failures == 0 ? 0 : 1;
}
