#include "clpp/clir.hpp"
#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <atomic>
#include <thread>

TEST_CASE("imported runtime source survives nested modules and CLIR", "[source-origin][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile("link \"./lib/wrapper.clp\" as Wrapper;\nWrapper.Run();\n",
      [](const std::string_view path) -> std::optional<std::string> {
        if (path == "./lib/wrapper.clp") return "link \"./broken.clp\" as Broken; export void Run() { Broken.Fail(); }";
        if (path == "./lib/broken.clp") return "export void Fail() {\n\n\n\n\n\n\n  report(\"module failure\");\n}";
        return std::nullopt;
      });
  for (const auto& diagnostic : compiled.diagnostics) INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  REQUIRE(compiled.chunk.source_files.size() == 3);
  for (const auto& chunk : {compiled.chunk, clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk))}) {
    REQUIRE(chunk.version != 0);
    clpp::VirtualMachine vm;
    vm.load(chunk);
    REQUIRE_FALSE(vm.run());
    CHECK(vm.error() == "module failure");
    const auto location = vm.error_location();
    CHECK(location.line == 8);
    REQUIRE(location.source < chunk.source_files.size());
    CHECK(chunk.source_files[location.source] == "./lib/broken.clp");
  }
  auto invalid = compiled.chunk;
  REQUIRE_FALSE(invalid.source_map.empty());
  invalid.source_map.front().location.source = static_cast<std::uint32_t>(invalid.source_files.size());
  CHECK_THROWS_AS(clpp::serialize_clir(invalid), std::invalid_argument);
}

TEST_CASE("entry runtime locations do not inherit a previous compilation", "[source-origin]") {
  for (int padding : {3, 7, 0, 12}) {
    const auto compiled = clpp::Compiler{}.compile(std::string(static_cast<std::size_t>(padding), '\n') + "report(\"entry failure\");");
    REQUIRE(compiled.ok());
    clpp::VirtualMachine vm;
    vm.load(compiled.chunk);
    REQUIRE_FALSE(vm.run());
    CHECK(vm.error_location().line == static_cast<std::uint32_t>(padding + 1));
    CHECK(vm.error_location().source == 0);
  }
}

TEST_CASE("type diagnostics retain expected and received types", "[diagnostic-types]") {
  for (const auto* source : {
      "number count = \"bad\";",
      "void Take(number value) {} Take(\"bad\");",
      "number Read() { return \"bad\"; }",
      "def Item { number Value; } auto item = Item { Value = \"bad\" };",
      "number count = 0; count = \"bad\";"}) {
    INFO(source);
    const auto result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    bool detailed = false;
    for (const auto& diagnostic : result.diagnostics) {
      if (diagnostic.message != "type mismatch") continue;
      CHECK(diagnostic.expected_type == "number");
      CHECK(diagnostic.actual_type == "\"bad\"");
      CHECK(diagnostic.formatted_message() == "type mismatch: expected number, received \"bad\"");
      detailed = true;
    }
    REQUIRE(detailed);
  }
  const auto joined = clpp::Compiler{}.compile("join(\"handle\");");
  REQUIRE_FALSE(joined.ok());
  REQUIRE_FALSE(joined.diagnostics.empty());
  CHECK(joined.diagnostics.front().formatted_message() ==
        "type mismatch: expected numeric thread handle for join, received \"handle\"");
  // Dynamic values retain their existing runtime-checking contract.
  CHECK(clpp::Compiler{}.compile("any Read() { return \"handle\"; } auto handle = Read(); join(handle);").ok());
}

TEST_CASE("concurrent compilations do not exchange source map origins", "[source-origin][compiler-reentrancy]") {
  std::atomic<int> failures{0};
  std::vector<std::thread> workers;
  for (int index = 0; index < 6; ++index) workers.emplace_back([index, &failures] {
    const auto padding = static_cast<std::size_t>(index * 37 + 10);
    const auto source = std::string(padding, '\n') + "report(\"thread-local origin\");";
    for (int iteration = 0; iteration < 40; ++iteration) {
      const auto result = clpp::Compiler{}.compile(source);
      if (!result.ok()) { ++failures; continue; }
      clpp::VirtualMachine vm;
      vm.load(result.chunk);
      if (vm.run() || vm.error_location().line != padding + 1 || vm.error_location().source != 0) ++failures;
    }
  });
  for (auto& worker : workers) worker.join();
  CHECK(failures.load() == 0);
}
