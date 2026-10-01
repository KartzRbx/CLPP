#include "clpp/compiler.hpp"
#include "clpp/core/lexer/lexer.hpp"
#include "clpp/ide.hpp"
#include "clpp/vm.hpp"
#include "core/vm/gc.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Tally {
  const char* name;
  std::uint64_t ran{0};
  std::uint64_t failed{0};
  double milliseconds{0};
};

int g_shown = 0;

void note(const std::string_view text) {
  if (g_shown >= 12) {
    return;
  }
  ++g_shown;
  std::cerr << "fail: " << text << '\n';
}

class Runner {
 public:
  Runner() { vm.set_output(out); }

  [[nodiscard]] bool output_is(const std::string& source, const std::string& expected) {
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (!compiled.ok() || compiled.chunk.code.empty()) {
      note(compiled.diagnostics.empty() ? "compile failed" : compiled.diagnostics[0].message);
      return false;
    }
    out.str({});
    out.clear();
    vm.load(compiled.chunk);
    if (!vm.run()) {
      note(vm.error().empty() ? "run failed" : vm.error());
      return false;
    }
    return out.str() == expected;
  }

  [[nodiscard]] bool rejects(const std::string& source) {
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (compiled.ok() || !compiled.chunk.code.empty() || compiled.diagnostics.empty()) {
      note("expected a diagnostic and no code");
      return false;
    }
    for (const clpp::Diagnostic& diagnostic : compiled.diagnostics) {
      if (diagnostic.message.empty() || diagnostic.location.line < 1) {
        note("diagnostic missing line or message");
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool survives(const std::string& source) {
    const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
    if (!compiled.diagnostics.empty() && !compiled.chunk.code.empty()) {
      note("emitted code while diagnostics remain");
      return false;
    }
    if (!compiled.ok()) {
      return true;
    }
    out.str({});
    out.clear();
    vm.load(compiled.chunk);
    (void)vm.run();
    return true;
  }

 private:
  std::ostringstream out;
  clpp::VirtualMachine vm;
};

[[nodiscard]] std::string nstr(const int value) { return std::to_string(value); }

[[nodiscard]] double elapsed_ms(const std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

Tally lex_phase(const int count) {
  Tally tally{"lexer"};
  const auto start = std::chrono::steady_clock::now();
  std::string source;
  source.reserve(128);
  for (int i = 0; i < count; ++i) {
    source.clear();
    const int kind = i % 6;
    if (kind == 0) {
      source = "post(" + nstr(i % 1000) + " + 2);\n";
    } else if (kind == 1) {
      source = "<< sim " + nstr(i) + "\nlet mut x = " + nstr(i % 50) + ";\n";
    } else if (kind == 2) {
      source = "func add(int a, int b) { return a + b; }\n";
    } else if (kind == 3) {
      source = "\"unterminated";
    } else if (kind == 4) {
      source = std::string("a\r\nb\0c", 6);
    } else {
      source = "<<[ block " + nstr(i % 3) + " ]>>\npost(1);\n";
    }
    clpp::Lexer lexer(source);
    const clpp::LexResult result = lexer.tokenize();
    bool ok = !result.tokens.empty() && result.tokens.back().type == clpp::TokenType::Eof;
    for (const clpp::Token& token : result.tokens) {
      if (token.location.line < 1 || token.location.column < 1) {
        ok = false;
      }
    }
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      if (diagnostic.message.empty() || diagnostic.location.line < 1) {
        ok = false;
      }
    }
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
      note("lexer");
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally conform_phase(Runner& runner, const int count) {
  Tally tally{"conform"};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    const int n = i % 40;
    const int limit = (i % 12) + 1;
    const int sum = limit * (limit - 1) / 2;
    bool ok = false;
    switch (i % 12) {
      case 0:
        ok = runner.output_is("post(" + nstr(n) + " + 2 * 3);\n", nstr(n + 6) + "\n");
        break;
      case 1:
        ok = runner.output_is("let mut x = " + nstr(n) + ";\nx = x + 1;\npost(x);\n", nstr(n + 1) + "\n");
        break;
      case 2:
        ok = runner.output_is("let mut s = 0;\nfor (let i in " + nstr(limit) + ") { s = s + i; }\npost(s);\n",
                              nstr(sum) + "\n");
        break;
      case 3:
        ok = runner.output_is("let mut s = 0;\nlet mut i = 0;\nwhile (i < " + nstr(limit) +
                                  ") { s = s + i;\ni = i + 1; }\npost(s);\n",
                              nstr(sum) + "\n");
        break;
      case 4:
        ok = runner.output_is("func add(int a, int b) { return a + b; }\npost(add(" + nstr(n) + ", 1));\n",
                              nstr(n + 1) + "\n");
        break;
      case 5:
        ok = runner.output_is("post(Vector3(" + nstr(n) + ", 4, 5).x);\n", nstr(n) + "\n");
        break;
      case 6:
        ok = runner.output_is("post(\"AZ\"[" + nstr(n % 2) + "]);\n", n % 2 == 0 ? "65\n" : "90\n");
        break;
      case 7:
        ok = runner.output_is("constexpr x = " + nstr(n) + " + 2;\npost(x);\n", nstr(n + 2) + "\n");
        break;
      case 8:
        ok = runner.output_is("post(\"v\" .: " + nstr(n) + ");\n", "v" + nstr(n) + "\n");
        break;
      case 9:
        ok = runner.output_is("struct Box { int n; }\nBox b = Box(" + nstr(n) + ");\npost(b.n);\n", nstr(n) + "\n");
        break;
      case 10:
        ok = runner.output_is(
            "struct Animal { int age; }\nstruct Dog : Animal { int bones; }\nDog pet = Dog(" + nstr(n) + ", " +
                nstr(n % 5) + ");\npost(pet.age);\npost(pet.bones);\n",
            nstr(n) + "\n" + nstr(n % 5) + "\n");
        break;
      default:
        ok = runner.output_is("observable int coins = " + nstr(n) +
                                  ";\ncoins.OnChange(func (int v) { post(v); });\ncoins = " + nstr(n + 1) + ";\n",
                              nstr(n + 1) + "\n");
        break;
    }
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally error_phase(Runner& runner, const int count) {
  Tally tally{"diagnostics"};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    bool ok = false;
    switch (i % 8) {
      case 0:
        ok = runner.rejects("let x = 1;\nlet x = 2;\n");
        break;
      case 1:
        ok = runner.rejects("let x = 1;\nx = 2;\n");
        break;
      case 2:
        ok = runner.rejects("post(missing());\n");
        break;
      case 3:
        ok = runner.rejects("int x = \"hi\";\n");
        break;
      case 4:
        ok = runner.rejects("let a =\npost(1);\n");
        break;
      case 5:
        ok = runner.rejects("struct D : B, C { int n; }\n");
        break;
      case 6:
        ok = runner.rejects("<<!strict\nfunc f(x) { return x; }\n");
        break;
      default:
        ok = runner.rejects("<<!strict\ntype State = \"Idle\" | \"Running\";\nlet state: State = \"Dead\";\n");
        break;
    }
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally fuzz_phase(Runner& runner, const int count) {
  Tally tally{"fuzz"};
  const auto start = std::chrono::steady_clock::now();
  std::string source;
  source.reserve(96);
  for (int i = 0; i < count; ++i) {
    source.clear();
    if (i % 3 == 0) {
      source = "<< ";
      source.push_back(static_cast<char>(32 + (i % 90)));
      source.push_back(static_cast<char>(32 + ((i / 3) % 90)));
      source += " \xff\npost(" + nstr(i % 20) + ");\n";
    } else if (i % 3 == 1) {
      source = "let a =\n#\nint x = " + nstr(i % 9) + ";\npost(x);\n";
    } else {
      source = "post(\"open\npost(1);\n";
    }
    ++tally.ran;
    if (!runner.survives(source)) {
      ++tally.failed;
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally ide_phase(const int count) {
  Tally tally{"ide"};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    const std::string source = "let score = " + nstr(i % 100) + ";\npost(score);\n";
    const clpp::ide::Info info = clpp::ide::inspect(source, 1, 6);
    bool ok = true;
    for (const clpp::Diagnostic& diagnostic : info.diagnostics) {
      if (diagnostic.message.empty()) {
        ok = false;
      }
    }
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
      note("ide diagnostic");
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally runtime_phase(Runner& runner, const int count) {
  Tally tally{"runtime"};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    bool ok = false;
    const int kind = i % 20;
    if (kind == 0) {
      ok = runner.output_is(
          "func step() {\n  coroutine.yield(1);\n  return 2;\n}\nlet co = coroutine.create(step);\n"
          "post(coroutine.resume(co));\npost(coroutine.resume(co));\n",
          "1\n2\n");
    } else if (kind == 1) {
      ok = runner.output_is("func twice(int n) { return n + n; }\npost(actor(twice, " + nstr(i % 30) + "));\n",
                            nstr((i % 30) * 2) + "\n");
    } else if (kind == 2) {
      ok = runner.output_is("post(pcall(1 / " + nstr(i % 2) + "));\n", i % 2 == 0 ? "0\n" : "1\n");
    } else if (kind == 3) {
      ok = runner.output_is("@coins = " + nstr(i % 15) + ";\n@this.coins = @coins + 5;\npost(@coins);\n",
                            nstr((i % 15) + 5) + "\n");
    } else if (kind == 4) {
      const clpp::CompileResult compiled = clpp::Compiler{}.compile("func dive() { dive(); }\ndive();\n");
      if (!compiled.ok()) {
        ok = false;
      } else {
        std::ostringstream ignored;
        clpp::VirtualMachine deep;
        deep.set_output(ignored);
        deep.load(compiled.chunk);
        ok = !deep.run() && deep.error() == "stack overflow";
      }
    } else {
      ok = runner.output_is(
          "extern func strlen(string s);\nlet v = list(3, 1, 2);\nlet s = sort(v);\npost(s[0]);\npost(strlen(\"CL++\"));\n",
          "1\n4\n");
    }
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally gc_phase(const int count) {
  Tally tally{"gc"};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    std::vector<std::unique_ptr<clpp::Table>> heap;
    clpp::Table* const live = clpp::vm::allocate(heap);
    clpp::Table* const other = clpp::vm::allocate(heap);
    (void)clpp::vm::allocate(heap);
    live->entries.emplace_back("other", clpp::Value::table_of(other));
    other->entries.emplace_back("live", clpp::Value::table_of(live));
    clpp::Value root = clpp::Value::table_of(live);
    clpp::vm::mark_value(root);
    clpp::vm::sweep(heap);
    bool ok = heap.size() == 2;
    root = clpp::Value::number_of(0);
    for (const std::unique_ptr<clpp::Table>& object : heap) {
      object->marked = false;
    }
    clpp::vm::mark_value(root);
    clpp::vm::sweep(heap);
    ok = ok && heap.empty();
    ++tally.ran;
    if (!ok) {
      ++tally.failed;
      note("gc");
    }
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

Tally axiom_phase(Runner& runner) {
  Tally tally{"axiom"};
  constexpr int kIters = 150000;
  constexpr int kChecks = 4;
  const auto start = std::chrono::steady_clock::now();
  const std::string source =
      "link @clpp.Axion as Axiom;\n"
      "let mut i = 0;\n"
      "let mut bad = 0;\n"
      "let mut sum = 0;\n"
      "while (i < 150000) {\n"
      "  let mut x = i % 100;\n"
      "  let mut c = Axiom.Clamp(x, 0, 10);\n"
      "  sum = sum + c;\n"
      "  if (x <= 10) { if (c != x) { bad = bad + 1; } }\n"
      "  if (x > 10) { if (c != 10) { bad = bad + 1; } }\n"
      "  if (Axiom.Hypot(3, 4) != 5) { bad = bad + 1; }\n"
      "  if (Axiom.Gcd(12, 8) != 4) { bad = bad + 1; }\n"
      "  if (Axiom.Lerp(0, 10, 0) != 0) { bad = bad + 1; }\n"
      "  i = i + 1;\n"
      "}\n"
      "post(bad);\n"
      "post(sum);\n";
  const bool ok = runner.output_is(source, "0\n1.4175e+06\n");
  tally.ran = static_cast<std::uint64_t>(kIters) * kChecks;
  if (!ok) {
    tally.failed = tally.ran;
    note("axiom loop");
  }
  tally.milliseconds = elapsed_ms(start);
  return tally;
}

struct Bench {
  const char* name;
  int operations;
  double milliseconds;
};

Bench bench_compile(const int count) {
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    const clpp::CompileResult compiled = clpp::Compiler{}.compile("post(1 + 2 * 3);\n");
    if (!compiled.ok()) {
      return Bench{"compile post", count, -1};
    }
  }
  return Bench{"compile post", count, elapsed_ms(start)};
}

Bench bench_run(const int count) {
  const clpp::CompileResult compiled = clpp::Compiler{}.compile("post(1 + 2 * 3);\n");
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    out.str({});
    out.clear();
    vm.load(compiled.chunk);
    if (!vm.run() || out.str() != "7\n") {
      return Bench{"run post", count, -1};
    }
  }
  return Bench{"run post", count, elapsed_ms(start)};
}

Bench bench_axiom(const int iters) {
  const std::string source = "link @clpp.Axion as Axiom;\nlet mut i = 0;\nlet mut s = 0;\nwhile (i < " + nstr(iters) +
                             ") {\n  s = s + Axiom.Clamp(i % 20, 0, 10);\n  i = i + 1;\n}\npost(s);\n";
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source);
  if (!compiled.ok()) {
    return Bench{"axiom clamp", iters, -1};
  }
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(compiled.chunk);
  const auto start = std::chrono::steady_clock::now();
  const bool ran = vm.run();
  const double ms = elapsed_ms(start);
  if (!ran) {
    return Bench{"axiom clamp", iters, -1};
  }
  return Bench{"axiom clamp", iters, ms};
}

void print_tally(const Tally& tally) {
  std::cout << tally.name << " ran " << tally.ran << " failed " << tally.failed << " ms " << tally.milliseconds
            << std::endl;
}

}  // namespace

int main() {
  Runner runner;
  std::vector<Tally> phases;
  phases.push_back(lex_phase(900000));
  print_tally(phases.back());
  phases.push_back(conform_phase(runner, 250000));
  print_tally(phases.back());
  phases.push_back(error_phase(runner, 200000));
  print_tally(phases.back());
  phases.push_back(fuzz_phase(runner, 150000));
  print_tally(phases.back());
  phases.push_back(ide_phase(40000));
  print_tally(phases.back());
  phases.push_back(runtime_phase(runner, 30000));
  print_tally(phases.back());
  phases.push_back(gc_phase(80000));
  print_tally(phases.back());
  phases.push_back(axiom_phase(runner));
  print_tally(phases.back());

  std::uint64_t ran = 0;
  std::uint64_t failed = 0;
  for (const Tally& tally : phases) {
    ran += tally.ran;
    failed += tally.failed;
  }

  const Bench compile = bench_compile(10000);
  const Bench run = bench_run(10000);
  const Bench axiom = bench_axiom(20000);
  std::cout << "benchmark " << compile.name << " ops " << compile.operations << " ms " << compile.milliseconds << std::endl;
  std::cout << "benchmark " << run.name << " ops " << run.operations << " ms " << run.milliseconds << std::endl;
  std::cout << "benchmark " << axiom.name << " ops " << axiom.operations << " ms " << axiom.milliseconds << std::endl;
  std::cout << "simulations " << ran << " failures " << failed << std::endl;
  return failed == 0 && compile.milliseconds >= 0 && run.milliseconds >= 0 && axiom.milliseconds >= 0 ? 0 : 1;
}
