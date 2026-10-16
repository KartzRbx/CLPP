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

[[nodiscard]] std::string mark(const int i) { return "<< env " + std::to_string(i) + "\n"; }

}  // namespace

int main() {
  constexpr int kRuns = 5000;
  int failures = 0;
  for (int i = 0; i < kRuns; ++i) {
    const int age = (i % 50) + 1;
    const int bones = (i % 7) + 1;
    const int toys = (i % 9) + 3;
    const int extra = i % 20;
    const int kind = i % 6;
    bool ok = false;
    if (kind == 0) {
      ok = expect_output(mark(i) + "def Box { number n; }\nBox b = Box { n = " + std::to_string(age) + " };\npost(b.n);\n",
                         std::to_string(age) + "\n");
    } else if (kind == 1) {
      ok = expect_output(mark(i) + "def Animal { number age; }\ndef Dog : Animal { number bones; }\nDog pet = Dog { age = " +
                             std::to_string(age) + ", bones = " + std::to_string(bones) + " };\npost(pet.age);\npost(pet.bones);\n",
                         std::to_string(age) + "\n" + std::to_string(bones) + "\n");
    } else if (kind == 2) {
      ok = expect_output(
          mark(i) + "def Animal { number age; number speak() { return @this.age; } }\n"
                    "def Dog : Animal { number bones; number speak() { return @this.bones; } }\n"
                    "Dog pet = Dog { age = " +
              std::to_string(age) + ", bones = " + std::to_string(bones) +
              " };\nAnimal view = pet;\npost(view.speak());\npost(view.age);\n",
          std::to_string(bones) + "\n" + std::to_string(age) + "\n");
    } else if (kind == 3) {
      ok = expect_output(
          mark(i) + "def Animal { number age; number speak() { return 1; } }\n"
                    "def Dog : Animal { number bones; }\n"
                    "def Puppy : Dog { number toys; number speak() { return @this.toys; } }\n"
                    "Puppy pup = Puppy { age = " +
              std::to_string(age) + ", bones = " + std::to_string(bones) + ", toys = " + std::to_string(toys) +
              " };\nAnimal any = pup;\npost(any.speak());\npost(any.age);\n",
          std::to_string(toys) + "\n" + std::to_string(age) + "\n");
    } else if (kind == 4) {
      ok = expect_output(
          mark(i) + "def Counter { number add(number x) { return x; } }\n"
          "def Plus : Counter { number add(number x) { return x + 1; } }\n"
          "Plus extra = Plus {  };\n"
          "Counter base = extra;\n"
          "post(base.add(" +
              std::to_string(extra) + "));\n",
          std::to_string(extra + 1) + "\n");
    } else if (i % 2 == 0) {
      ok = expect_fail(mark(i) + "def Dog : Missing { number bones; }\n");
    } else {
      ok = expect_fail(mark(i) + "def Animal { number age; }\ndef Dog : Animal { number age; }\n");
    }
    if (!ok) {
      ++failures;
    }
  }
  std::cout << "ran " << kRuns << " failures " << failures << '\n';
  return failures == 0 ? 0 : 1;
}
