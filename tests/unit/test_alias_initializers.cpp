#include "clpp/compiler.hpp"
#include "clpp/ide.hpp"
#include "clpp/vm.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <algorithm>

namespace {
void executes(const std::string& source, const std::string& expected, const clpp::ModuleLoader& loader = {}) {
  const auto compiled = clpp::Compiler{}.compile(source, loader);
  for (const auto& diagnostic : compiled.diagnostics) INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  std::ostringstream output;
  clpp::VirtualMachine vm;
  vm.set_output(output);
  vm.load(compiled.chunk);
  REQUIRE(vm.run());
  CHECK(output.str() == expected);
}
}

TEST_CASE("data aliases construct local chained and empty definitions", "[alias-initializer][regression]") {
  executes("def Account { number Amount = 10; number Grant(number amount) { @this.Amount += amount; return @this.Amount; } } "
           "def A = Account; def B = A; B account = B {}; post(account.Grant(5)); "
           "def Empty {} def E = Empty; E item = E {}; post(item:is<Empty>);", "15\ntrue\n");
}

TEST_CASE("data aliases construct concrete and formal generic definitions", "[alias-initializer][regression]") {
  executes("def Box<T> { T Value; } def NumberBox = Box<number>; def BoxOf<T> = Box<T>; "
           "template<typename T> Box<T> Make(T value) { return BoxOf<T> { Value = value }; } "
           "auto a = NumberBox { Value = 3 }; auto b = BoxOf<string> { Value = \"text\" }; "
           "post(a.Value); post(b.Value); post(Make<number>(7).Value);", "3\ntext\n7\n");
}

TEST_CASE("data aliases construct imported types and retain defaults methods and identity", "[alias-initializer][modules]") {
  const clpp::ModuleLoader loader = [](std::string_view) -> std::optional<std::string> {
    return "export def Account { private number Amount = 10; optional<number> Limit; "
           "number Balance() { return @this.Amount; } } export def AccountAlias = Account;";
  };
  executes("link \"./Wallet.clp\" as Wallet; def _Wallet = Wallet.Account; "
           "_Wallet account = _Wallet {}; Wallet.Account original = account; "
           "post(account.Balance()); post(account.Limit); post(original == account); "
           "auto another = Wallet.AccountAlias {}; post(another.Balance());", "10\nnull\ntrue\n10\n", loader);
}

TEST_CASE("data alias construction preserves field and abstract restrictions", "[alias-initializer][regression]") {
  for (const char* source : {
      "def A = number; auto a = A {};",
      "def Data {} def A = optional<Data>; auto a = A {};",
      "def Data {} def A = Data | number; auto a = A {};",
      "def Data { number Required; } def A = Data; auto a = A {};",
      "def Data { number Required; } def A = Data; auto a = A { Required = \"bad\" };",
      "def Data { private number Secret = 1; } def A = Data; auto a = A {}; post(a.Secret);",
      "abstract def Data { abstract number Read(); } def A = Data; auto a = A {};"}) {
    INFO(source);
    CHECK_FALSE(clpp::Compiler{}.compile(source).ok());
  }
}

TEST_CASE("imported data aliases retain initializer and method completion", "[alias-initializer][ide]") {
  const clpp::ModuleLoader loader = [](std::string_view) -> std::optional<std::string> {
    return "export def Account { number Amount = 0; number Grant(number value) { return value; } }";
  };
  const std::string prefix = "link \"./Wallet.clp\" as Wallet;\ndef _Wallet = Wallet.Account;\n";
  const std::string initializer = "_Wallet account = _Wallet { ";
  const auto fields = clpp::ide::inspect(prefix + initializer, 3, static_cast<std::uint32_t>(initializer.size() + 1), loader);
  CHECK(std::any_of(fields.completions.begin(), fields.completions.end(), [](const auto& item) {
    return item.label == "Amount" && item.detail.find("number") != std::string::npos;
  }));
  const auto members = clpp::ide::inspect(prefix + "_Wallet account = _Wallet {};\naccount.", 4, 9, loader);
  for (const std::string label : {"Amount", "Grant"}) {
    INFO(label);
    CHECK(std::any_of(members.completions.begin(), members.completions.end(), [&](const auto& item) {
      return item.label == label;
    }));
  }
}
