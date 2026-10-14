#include "clpp/compiler.hpp"
#include "clpp/bytecode_verifier.hpp"
#include "clpp/clir.hpp"
#include "clpp/register_ir.hpp"
#include "clpp/stack_verifier.hpp"
#include "core/compiler/bytecode_format.hpp"
#include "clpp/repl.hpp"
#include "clpp/stdlib.hpp"
#include "clpp/vm.hpp"
#include "core/compiler/opcode.hpp"
#include "core/vm/gc.hpp"
#include "stdlib/host.hpp"
#include "stdlib/gfx_text.hpp"
#include "stdlib/ui_style.hpp"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdlib>
#include <stdexcept>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>

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


TEST_CASE("string escapes axiom floor and program args", "[pipeline]") {
  const std::string source =
      "link @clpp.axiom as Axiom;\n"
      "link @clpp.text as Text;\n"
      "post(\"a\\n\");\n"
      "post(Text.trim(\"  hi  \"));\n"
      "post(Text.contains(\"abcd\", \"bc\"));\n"
      "post(Axiom.Floor(3.8));\n"
      "post(Axiom.IsNaN(1));\n"
      "const auto values = args();\n"
      "post(values[0]);\n"
      "post(values[1]);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  if (!result.ok()) {
    std::string notes;
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      notes += diagnostic.message;
      notes += "\n";
    }
    INFO(notes);
  }
  REQUIRE(result.ok());
  clpp::stdlib::set_program_args({"one", "two"});
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "a\n\nhi\ntrue\n3\nfalse\none\ntwo\n");
}

TEST_CASE("missing return and unreachable code", "[pipeline]") {
  const clpp::CompileResult missing = clpp::Compiler{}.compile("number f() { post(1); }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "missing return");
  const clpp::CompileResult dead = clpp::Compiler{}.compile("number f() { return 1; post(2); }\n");
  REQUIRE_FALSE(dead.ok());
  bool unreachable = false;
  for (const clpp::Diagnostic& diagnostic : dead.diagnostics) {
    unreachable = unreachable || diagnostic.message == "unreachable code";
  }
  REQUIRE(unreachable);
}

TEST_CASE("vm loads empty chunk and runs without throwing", "[vm]") {
  clpp::BytecodeChunk chunk;
  clpp::VirtualMachine vm;
  REQUIRE_NOTHROW(vm.load(chunk));
  REQUIRE_NOTHROW(vm.run());
}

TEST_CASE("vm posts a constant string", "[vm]") {
  clpp::BytecodeChunk chunk;
  chunk.constants.push_back(clpp::Value::string_of("ab"));
  chunk.code.push_back(static_cast<std::uint8_t>(clpp::compiler::Opcode::Const));
  chunk.code.push_back(0);
  chunk.code.push_back(0);
  chunk.code.push_back(static_cast<std::uint8_t>(clpp::compiler::Opcode::Post));
  chunk.code.push_back(static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt));

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "ab\n");
}

TEST_CASE("compile post hello and concat", "[pipeline]") {
  const std::string source = "post(\"hello\");\npost(\"a\" .: \"b\");\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  REQUIRE(result.chunk.constants.size() == 3);
  REQUIRE(result.chunk.constants[0].is_string());
  REQUIRE(result.chunk.constants[0].text == "hello");
  REQUIRE(result.chunk.constants[1].text == "a");
  REQUIRE(result.chunk.constants[2].text == "b");

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "hello\nab\n");
}

TEST_CASE("compile refuses a source with diagnostics", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("spawn work;");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
}

TEST_CASE("compile arithmetic precedence", "[pipeline]") {
  const std::string source = "post(1 + 2 * 3);\npost((1 + 2) * 3);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "7\n9\n");
}

TEST_CASE("compile refuses incomplete arithmetic", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(1 +);");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "expected expression");
}

TEST_CASE("compile inferred constant and post the file binding", "[pipeline]") {
  const std::string source = "const auto x = 1 + 2;\npost(x);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  REQUIRE(result.chunk.local_count == 0);

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile assignment updates the local", "[pipeline]") {
  const std::string source = "auto x = 1;\nx = x + 1;\npost(x);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "2\n");
}

TEST_CASE("compile rejects undefined and redeclared names", "[pipeline]") {
  const clpp::CompileResult undefined_name = clpp::Compiler{}.compile("post(x);");
  REQUIRE_FALSE(undefined_name.ok());
  REQUIRE(undefined_name.chunk.code.empty());
  REQUIRE(undefined_name.diagnostics[0].message == "undefined name");

  const clpp::CompileResult redeclared = clpp::Compiler{}.compile("const auto x = 1; const auto x = 2;");
  REQUIRE_FALSE(redeclared.ok());
  REQUIRE(redeclared.diagnostics[0].message == "already declared");
}

TEST_CASE("compile while sums one through ten", "[pipeline]") {
  const std::string source =
      "auto total = 0;\n"
      "auto i = 1;\n"
      "while (i <= 10) {\n"
      "  total = total + i;\n"
      "  i = i + 1;\n"
      "}\n"
      "post(total);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "55\n");
}

TEST_CASE("compile if else and short-circuit and", "[pipeline]") {
  const clpp::CompileResult branch = clpp::Compiler{}.compile("if (0) { post(1); } else { post(2); }");
  REQUIRE(branch.ok());
  std::ostringstream branched;
  clpp::VirtualMachine vm;
  vm.set_output(branched);
  vm.load(branch.chunk);
  REQUIRE(vm.run());
  REQUIRE(branched.str() == "2\n");

  const clpp::CompileResult skipped = clpp::Compiler{}.compile("if (0 and (1 / 0)) { post(1); } else { post(2); }");
  REQUIRE(skipped.ok());
  std::ostringstream safe;
  clpp::VirtualMachine other;
  other.set_output(safe);
  other.load(skipped.chunk);
  REQUIRE(other.run());
  REQUIRE(safe.str() == "2\n");
}

TEST_CASE("compile not and comparison", "[pipeline]") {
  const std::string source = "if (not 0) { post(1); }\npost(1 + 2 == 3);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "1\ntrue\n");
}

TEST_CASE("compile local after a block-scoped constant", "[pipeline]") {
  const std::string source = "if (0) { const auto hidden = 1; }\nconst auto shown = 7;\npost(shown);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "7\n");
}

TEST_CASE("vm reports division by zero", "[vm]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(1 / 0);");
  REQUIRE(result.ok());
  clpp::VirtualMachine vm;
  vm.load(result.chunk);
  REQUIRE_FALSE(vm.run());
  REQUIRE(vm.error() == "division by zero");
}

TEST_CASE("compile function returns one plus two", "[pipeline]") {
  const std::string source = "number three() {\n  return 1 + 2;\n}\npost(three());\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile function with parameters", "[pipeline]") {
  const std::string source = "number add(number a, number b) {\n  return a + b;\n}\npost(add(1, 2));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile rejects a bad call", "[pipeline]") {
  const clpp::CompileResult missing = clpp::Compiler{}.compile("post(missing());");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "undefined function");

  const clpp::CompileResult arity = clpp::Compiler{}.compile("number one(number a) { return a; }\npost(one());");
  REQUIRE_FALSE(arity.ok());
  REQUIRE(arity.diagnostics[0].message == "wrong number of arguments");
}

TEST_CASE("compile vector math and buffer size", "[pipeline]") {
  const std::string source =
      "void UpdatePosition(Vector3 velocity) {\n"
      "  Vector3 currentPos = Vector3(0.0, 10.0, 0.0);\n"
      "  Vector3 nextPos = currentPos + (velocity * 0.016);\n"
      "  post(\"New Pos: \" .: nextPos.x .: \", \" .: nextPos.y .: \", \" .: nextPos.z);\n"
      "}\n"
      "void ProcessPacket() {\n"
      "  buffer stream = buffer::create(256);\n"
      "  buffer::write_string(stream, 0, \"CL++ Data\");\n"
      "  post(\"Buffer allocated with size: \" .: buffer::size(stream));\n"
      "}\n"
      "UpdatePosition(Vector3(1.0, 0.0, 0.0));\n"
      "ProcessPacket();\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "New Pos: 0.016, 10, 0\nBuffer allocated with size: 256\n");
}

TEST_CASE("examples/vectors_buffers.clp runs", "[pipeline]") {
  // `link @clpp.roblox;` used to be ignored silently; a link to a missing module is now an error.
  REQUIRE_FALSE(clpp::Compiler{}.compile("link @clpp.roblox;\n").ok());
  const clpp::CompileResult result = clpp::Compiler{}.compile(
      "void UpdatePosition(Vector3 velocity) {\n"
      "  Vector3 currentPos = Vector3(0.0, 10.0, 0.0);\n"
      "  Vector3 nextPos = currentPos + (velocity * 0.016);\n"
      "  post(\"New Pos: \" .: nextPos.x .: \", \" .: nextPos.y .: \", \" .: nextPos.z);\n"
      "}\n"
      "void ProcessPacket() {\n"
      "  buffer stream = buffer::create(256);\n"
      "  buffer::write_string(stream, 0, \"CL++ Data\");\n"
      "  post(\"Buffer allocated with size: \" .: buffer::size(stream));\n"
      "}\n");
  REQUIRE(result.ok());
  clpp::VirtualMachine vm;
  vm.load(result.chunk);
  REQUIRE(vm.run());
}

TEST_CASE("compile rejects a type mismatch", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("number x = \"hi\";");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile accepts an annotated number", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("number x = 1 + 2;\npost(x);\n");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile struct field sum and enum variant", "[pipeline]") {
  const std::string source =
      "def Point {\n"
      "  number x;\n"
      "  number y;\n"
      "}\n"
      "enum class Color {\n"
      "  Red,\n"
      "  Green,\n"
      "  Blue\n"
      "}\n"
      "Point p = Point { x = 3, y = 4 };\n"
      "post(p.x + p.y);\n"
      "post(Color::Green);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "7\n1\n");
}

TEST_CASE("compile rejects a struct field type mismatch", "[pipeline]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("def Point { number x; number y; }\n"
      "Point p = Point { x = \"hi\", y = 1 };\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile rejects an unknown enum variant", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("enum class Color { Red }\npost(Color::Blue);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "unknown variant");
}

TEST_CASE("compile template interpolation", "[pipeline]") {
  const std::string source = "const auto name = \"CL++\";\npost(`Hello ${name}, ${1 + 2}`);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "Hello CL++, 3\n");
}

TEST_CASE("compile template escaped dollar stays literal", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(`\\${name}`);\n");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "${name}\n");
}

TEST_CASE("compile rejects a template assigned to number", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("number x = `v${1}`;\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile rejects an unterminated interpolation", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(`Hello ${name`);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "unterminated interpolation");
}

TEST_CASE("CLIR preserves booleans and rejects invalid boolean literals", "[runtime-bool][clir]") {
  const auto valid=clpp::Compiler{}.compile("post(array<bool>(true, false, 1<2));");
  REQUIRE(valid.ok());
  auto bytes=clpp::serialize_clir(valid.chunk);
  const auto restored=clpp::deserialize_clir(bytes);
  REQUIRE(clpp::verify_bytecode(restored).empty());
  std::ostringstream out; clpp::VirtualMachine vm; vm.set_output(out); vm.load(restored);
  REQUIRE(vm.run()); CHECK(out.str()=="[true, false, true]\n");
  auto malformed=valid.chunk;
  auto found=std::find_if(malformed.constants.begin(), malformed.constants.end(), [](const auto& value) { return value.is_boolean(); });
  REQUIRE(found!=malformed.constants.end()); found->number=2;
  CHECK_FALSE(clpp::verify_bytecode(malformed).empty());
  CHECK_THROWS_AS(clpp::serialize_clir(malformed), std::invalid_argument);
  REQUIRE(valid.chunk.constants.front().is_boolean());
  // Header (16), code, constant count (2), then the first tag and boolean byte.
  bytes[19+valid.chunk.code.size()]=2;
  CHECK_FALSE(clpp::verify_bytecode(clpp::deserialize_clir(bytes)).empty());
}

TEST_CASE("compile receiver field assignment and reads through @this", "[pipeline]") {
  const std::string source = R"(
    def Wallet { number Coins = 10; }
    void Wallet::Add(number amount) { @this.Coins = @this.Coins + amount; }
    number Wallet::Read() { return @this.Coins; }
    auto wallet = Wallet {};
    wallet.Add(5);
    post(wallet.Read());
  )";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "15\n");
}

TEST_CASE("compile reads a field through @this scope", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("number flag = 7;\npost(flag);\n");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "7\n");
}

TEST_CASE("mark-sweep keeps a cycle and drops garbage", "[gc]") {
  std::vector<std::unique_ptr<clpp::Table>> heap;
  clpp::Table* const live = clpp::vm::allocate(heap);
  clpp::Table* const other = clpp::vm::allocate(heap);
  live->entries.emplace_back("other", clpp::Value::table_of(other));
  other->entries.emplace_back("live", clpp::Value::table_of(live));
  clpp::Table* const garbage = clpp::vm::allocate(heap);
  REQUIRE(garbage != nullptr);
  REQUIRE(heap.size() == 3);

  clpp::Value root = clpp::Value::table_of(live);
  clpp::vm::mark_value(root);
  clpp::vm::sweep(heap);
  REQUIRE(heap.size() == 2);

  root = clpp::Value::number_of(0);
  for (const std::unique_ptr<clpp::Table>& object : heap) {
    object->marked = false;
  }
  clpp::vm::mark_value(root);
  clpp::vm::sweep(heap);
  REQUIRE(heap.empty());
}

TEST_CASE("compile link calls an exported function", "[pipeline]") {
  const std::string source = "link \"./math.clp\" as Math;\npost(Math.twice(4));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source, [](const std::string_view path) -> std::optional<std::string> {
    if (path == "./math.clp") {
      return std::string("number add(number a, number b) { return a + b; }\nexport number twice(number n) { return add(n, n); }\n");
    }
    return std::nullopt;
  });
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "8\n");
}

TEST_CASE("compile rejects a missing module", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("link \"./missing.clp\" as Math;\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "cannot open module");
}

TEST_CASE("compile match guard picks the passing arm", "[pipeline]") {
  const std::string source =
      "const auto score = 2;\n"
      "match (score) {\n"
      "  1 => post(10);\n"
      "  2 guard score > 10 => post(1);\n"
      "  2 guard score > 0 => post(20);\n"
      "  _ => post(0);\n"
      "}\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "20\n");
}

TEST_CASE("compile rejects a match on a string", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("match (\"a\") { 1 => post(1); }\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile await reads an async function", "[pipeline]") {
  const std::string source = "async number add(number a, number b) { return a + b; }\npost(await add(1, 2));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile rejects an async call used as number", "[pipeline]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("async number add(number a, number b) { return a + b; }\nnumber x = add(1, 2);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile spawn stores a task that await reads", "[pipeline]") {
  const std::string source =
      "async number add(number a, number b) { return a + b; }\n"
      "task<number> job = spawn add(1, 2);\n"
      "post(await job);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile rejects spawn stored as number", "[pipeline]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("async number add(number a, number b) { return a + b; }\nnumber x = spawn add(1, 2);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile parallel lists async results", "[pipeline]") {
  const std::string source =
      "async number add(number a, number b) { return a + b; }\n"
      "post(parallel(add(1, 2), add(10, 20)));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "[3, 30]\n");
}

TEST_CASE("ternary range slice bits explicit arguments try and lambda", "[pipeline]") {
  const std::string source =
      "number inc(number x) { return x + 1; }\n"
      "any first(any ... values) { return values[0]; }\n"
      "template <typename T> T id(T value) where T: number { return value; }\n"
      "number DemoPing() { return 4; }\n"
      "post(0 ? 2 : 3);\n"
      "post(1 && 0);\n"
      "post(0 || 4);\n"
      "post(6 & 3);\n"
      "post(1 | 2);\n"
      "post(1 shl 3);\n"
      "post(~0);\n"
      "auto n = 1;\n"
      "n++;\n"
      "post(n);\n"
      "post(\"abcd\"[1 .. 3]);\n"
      "post(inc(1));\n"
      "post(inc(4));\n"
      "post(first(7, 8));\n"
      "post((func(number x) -> number { return x + 1; })(2));\n"
      "array<number> xs = array<number>(1, 2, 3);\n"
      "post(xs[1]);\n"
      "dictionary<string, number> sheet = dictionary<string, number>(\"a\", 9);\n"
      "post(sheet[\"a\"]);\n"
      "number y = 5; if (y != 0) { post(y); }\n"
      "auto pair = list(8, 9); auto a = pair[0]; auto b = pair[1];\n"
      "post(a);\n"
      "post(b);\n"
      "post(DemoPing());\n"
      "post(id<number>(3));\n"
      "try { post(1 / 0); } catch (e) { post(0); }\n"
      "\n"
      "post(1);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  if (!result.ok()) {
    std::string notes;
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      notes += diagnostic.message;
      notes += "\n";
    }
    INFO(notes);
  }
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  const bool ran = vm.run();
  INFO(std::string(vm.error()));
  INFO(out.str());
  REQUIRE(ran);
  REQUIRE(out.str() == "3\nfalse\ntrue\n2\n3\n8\n-1\n2\nbc\n2\n5\n7\n3\n2\n9\n5\n8\n9\n4\n3\n0\n1\n");
}

TEST_CASE("private override abstract generics option and payload", "[pipeline]") {
  const std::string source =
      "abstract def Animal {\n"
      "  private number secret;\n"
      "  number reveal() { return @this.secret; }\n"
      "  abstract number speak();\n"
      "}\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "  override number speak() { return @this.bones; }\n"
      "}\n"
      "def Flyer { number lift; }\n"
      "abstract def Runner { abstract number Pace(); }\n"
      "def Hero : Flyer, Runner { number pace; number power; number Pace() { return @this.pace; } }\n"
      "def Box<T> { T item; }\n"
      "enum class MsgKind { Text, Num } def Msg { MsgKind Kind; string Text = \"\"; number Num = 0; }\n"
      "template <typename T> T id(T value) { return value; }\n"
      "Dog pet = Dog { secret = 9, bones = 4 };\n"
      "post(pet.reveal());\n"
      "post(pet.speak());\n"
      "Hero hero = Hero { lift = 3, pace = 4, power = 5 };\n"
      "post(hero.lift);\n"
      "post(hero.power);\n"
      "Box<number> box = Box<number> { item = 8 };\n"
      "post(box.item);\n"
      "post(id<number>(3));\n"
      "Msg note = Msg { Kind = MsgKind::Text, Text = \"hi\" };\n"
      "match (note.Kind) { MsgKind::Text => post(note.Text); MsgKind::Num => post(note.Num); }\n"
      "optional<number> item = 6;\n"
      "if (item != null) { post(item); } else { post(0); }\n"
      "try { report(\"no\"); } catch (error) { post(error); }\n"
      "const auto x = 1;\n"
      "if (1) { const auto x = 2; post(x); }\n"
      "post(x);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  if (!result.ok()) {
    std::string notes;
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      notes += diagnostic.message;
      notes += "\n";
    }
    INFO(notes);
  }
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  const bool ran = vm.run();
  INFO(vm.error());
  REQUIRE(ran);
  REQUIRE(out.str() == "9\n4\n3\n5\n8\n3\nhi\n6\nno\n2\n1\n");

  const clpp::CompileResult hidden =
      clpp::Compiler{}.compile("def Animal { private number secret; }\n"
      "Animal pet = Animal { secret = 1 };\n"
      "post(pet.secret);\n");
  REQUIRE_FALSE(hidden.ok());
  REQUIRE(hidden.diagnostics[0].message == "private member");

  const clpp::CompileResult bare = clpp::Compiler{}.compile("abstract def Animal { abstract number speak(); }\n"
  "Animal pet = Animal {  };\n");
  REQUIRE_FALSE(bare.ok());
  REQUIRE(bare.diagnostics[0].message == "abstract type");

  const clpp::CompileResult missing = clpp::Compiler{}.compile("def Dog { override number speak() { return 1; } }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "nothing to override");
}

TEST_CASE("async call waits until await and spawn runs it", "[pipeline]") {
  const clpp::CompileResult deferred = clpp::Compiler{}.compile(
      "async number later() { post(1); return 2; }\n"
      "const auto job = later();\n"
      "post(0);\n"
      "post(await job);\n");
  REQUIRE(deferred.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(deferred.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "0\n1\n2\n");

  const clpp::CompileResult spawned = clpp::Compiler{}.compile(
      "async number later() { post(7); return 8; }\n"
      "task<number> job = spawn later();\n"
      "post(await job);\n");
  REQUIRE(spawned.ok());
  std::ostringstream spawned_out;
  clpp::VirtualMachine spawned_vm;
  spawned_vm.set_output(spawned_out);
  spawned_vm.load(spawned.chunk);
  REQUIRE(spawned_vm.run());
  REQUIRE(spawned_out.str() == "7\n8\n");
}

TEST_CASE("compile rejects parallel of numbers", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(parallel(1, 2));\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("repl runs one statement", "[repl]") {
  std::istringstream in("post(1 + 2);\nexit\n");
  std::ostringstream out;
  std::ostringstream err;
  REQUIRE(clpp::run_repl(in, out, err) == 0);
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile stdlib abs", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("link @clpp.math as Math;\npost(Math.abs(0 - 4));\n");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "4\n");
}

TEST_CASE("compile fs size of a file", "[pipeline]") {
  const std::string name = "clpp_fs_size_probe.txt";
  {
    std::ofstream file(name, std::ios::binary);
    file << "abcd";
  }
  const std::string source = "link @clpp.fs as Fs;\npost(Fs.size(\"" + name + "\"));\npost(Fs.size(\"clpp_fs_missing.txt\"));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "4\n-1\n");
  std::filesystem::remove(name);
}

TEST_CASE("compile unary compound for break switch const", "[pipeline]") {
  const std::string source =
      "auto x = 4;\n"
      "post(-x);\n"
      "x += 1;\n"
      "post(x);\n"
      "const auto name = 7;\n"
      "post(name);\n"
      "if (true) { post(1); } else { post(0); }\n"
      "if (false) { post(8); } else { post(2); }\n"
      "if (null) { post(8); } else { post(3); }\n"
      "auto total = 0;\n"
      "for (auto k = 0; k < 4; k += 1) { total = total + k; }\n"
      "post(total);\n"
      "auto seen = 0;\n"
      "auto k = 0;\n"
      "while (k < 10) { if (k == 3) { break; } seen = seen + 1; k = k + 1; }\n"
      "post(seen);\n"
      "switch (2) {\n"
      "  case 1: post(8); break;\n"
      "  case 2: post(9); break; post(8);\n"
      "  default: post(8); break;\n"
      "}\n"
      "post(!0);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "-4\n5\n7\n1\n2\n3\n6\n3\n9\ntrue\n");
}

TEST_CASE("compile rejects assignment to const", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("const auto x = 1;\nx = 2;\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "cannot assign to immutable binding");
}

TEST_CASE("compile os env and http host", "[pipeline]") {
  set_test_env("CLPP_SIM", "42");
  const clpp::CompileResult result = clpp::Compiler{}.compile(
      "link @clpp.os as Os;\n"
      "link @clpp.http as Http;\n"
      "post(Os.env(\"CLPP_SIM\"));\n"
      "post(Http.host(\"http://h.test:80/a\"));\n");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "42\nh.test\n");
}

TEST_CASE("compile empty source halts with no output", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("");
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str().empty());
}

TEST_CASE("compile rejects assignment to inferred constant", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("const auto x = 1;\nx = 2;\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "cannot assign to immutable binding");
}

TEST_CASE("compile exhaustive enum match", "[pipeline]") {
  const std::string source =
      "enum class Color { Red, Green, Blue }\n"
      "const auto color = Color::Green;\n"
      "match (color) {\n"
      "  Color::Red => post(0);\n"
      "  Color::Green => post(1);\n"
      "  Color::Blue => post(2);\n"
      "}\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "1\n");
}

TEST_CASE("compile rejects non-exhaustive enum match", "[pipeline]") {
  const clpp::CompileResult missing = clpp::Compiler{}.compile(
      "enum class Color { Red, Green, Blue }\n"
      "const auto color = Color::Green;\n"
      "match (color) { Color::Red => post(0); }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.chunk.code.empty());
  REQUIRE(missing.diagnostics[0].message == "non-exhaustive match");

  const clpp::CompileResult guarded = clpp::Compiler{}.compile(
      "enum class Color { Red, Green }\n"
      "const auto color = Color::Red;\n"
      "match (color) {\n"
      "  Color::Red guard false => post(0);\n"
      "  Color::Green => post(1);\n"
      "}\n");
  REQUIRE_FALSE(guarded.ok());
  REQUIRE(guarded.diagnostics[0].message == "non-exhaustive match");

  const clpp::CompileResult wildcard = clpp::Compiler{}.compile(
      "enum class Color { Red, Green, Blue }\n"
      "const auto color = Color::Blue;\n"
      "match (color) { _ => post(2); }\n");
  REQUIRE(wildcard.ok());
}

TEST_CASE("compile the remaining statement syntax", "[pipeline]") {
  const std::string source =
      "link @clpp.math as Math;\n"
      "\n"
      "def Point { number x; number y; }\n"
      "signal<number> ping;\n"
      "void show(number x) { post(x); }\n"
      "number one() { return 1; }\n"
      "auto base = 3;\n"
      "observable number n = 1;\n"
      "n = n + 1;\n"
      "constexpr number doubled = 2 + 2;\n"
      "post(base);\n"
      "warn(n);\n"
      "try { report(doubled); } catch (e) { post(e); }\n"
      "post(Math.abs(0 - 5));\n"
      "post(\"\");\n"
      "Vector2 v = Vector2(1, 2);\n"
      "post(v.y);\n"
      "post(Vector4(1, 2, 3, 4).w);\n"
      "post(Vector2(8, 9)[1]);\n"
      "post(\"AZ\"[1]);\n"
      "auto sum = 0;\n"
      "for (auto i in 4) { sum = sum + i; }\n"
      "post(sum);\n"
      "post(pcall(1 / 0));\n"
      "post(pcall(8 / 2));\n"
      "ping~>Connect(show);\n"
      "ping::Fire(6);\n"
      "post(one());\n"
      "post(Point { x = 3, y = 4 }.x);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  std::ostringstream err;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.set_error_output(err);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n4\n5\n\n2\n4\n9\nZ\n6\n0\n1\n6\n1\n3\n");
  REQUIRE(err.str() == "warning: 2\n");

  const clpp::CompileResult folded = clpp::Compiler{}.compile("constexpr number x = name;\n");
  REQUIRE_FALSE(folded.ok());
  REQUIRE(folded.diagnostics[0].message == "not a constant");
}

TEST_CASE("register add stores into the destination register", "[register]") {
  const std::string source = "const auto a = 5;\nconst auto b = 30;\nconst auto c = a + b;\npost(c);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  const clpp::RegChunk lowered = clpp::lower_registers(result.chunk);
  REQUIRE(lowered.error.empty());
  bool found = false;
  for (const std::uint32_t word : lowered.code) {
    if (static_cast<clpp::RegOp>(clpp::reg_op(word)) == clpp::RegOp::IntAdd && clpp::reg_b(word) == 0 &&
        clpp::reg_c(word) == 1) {
      found = true;
    }
  }
  REQUIRE(found);
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "35\n");
}

TEST_CASE("register lowering rejects invalid jumps and stack underflow", "[register]") {
  clpp::BytecodeChunk unknown_unreachable;
  unknown_unreachable.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt), 0xFF};
  REQUIRE(clpp::verify_bytecode(unknown_unreachable) == "register opcode");
  REQUIRE(clpp::lower_registers(unknown_unreachable).error == "register opcode");

  clpp::BytecodeChunk invalid_constant;
  invalid_constant.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Const), 0, 0,
                           static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(invalid_constant) == "register constant");
  REQUIRE(clpp::lower_registers(invalid_constant).error == "register constant");

  clpp::BytecodeChunk invalid_function;
  invalid_function.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Call), 0, 0,
                           static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(invalid_function) == "register function");
  REQUIRE(clpp::lower_registers(invalid_function).error == "register function");

  clpp::BytecodeChunk invalid_local;
  invalid_local.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::LoadLocal), 0, 0,
                        static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(invalid_local) == "register local");
  REQUIRE(clpp::lower_registers(invalid_local).error == "register local");

  clpp::BytecodeChunk oversized;
  oversized.code.resize(0x10000, static_cast<std::uint8_t>(clpp::compiler::Opcode::Nop));
  REQUIRE(clpp::lower_registers(oversized).error == "register code too large");

  clpp::BytecodeChunk invalid_entry;
  invalid_entry.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  invalid_entry.entry = 1;
  REQUIRE(clpp::lower_registers(invalid_entry).error == "register entry");

  clpp::BytecodeChunk invalid_function_entry;
  invalid_function_entry.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  invalid_function_entry.functions.push_back(clpp::FunctionBytecode{0, 0, 1});
  REQUIRE(clpp::lower_registers(invalid_function_entry).error == "register entry");

  clpp::BytecodeChunk out_of_range;
  out_of_range.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Jump), 0xFF, 0xFF,
                       static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::lower_registers(out_of_range).error == "register target");

  clpp::BytecodeChunk into_operand;
  into_operand.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Jump), 1, 0,
                       static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::lower_registers(into_operand).error == "register target");

  clpp::BytecodeChunk underflow;
  underflow.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Post),
                    static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::lower_registers(underflow).error == "register stack underflow");
}

TEST_CASE("bytecode verifier enforces the chunk format version", "[bytecode]") {
  clpp::BytecodeChunk chunk;
  chunk.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(chunk).empty());

  chunk.version = 0;
  REQUIRE(clpp::verify_bytecode(chunk) == "bytecode version");
}

TEST_CASE("decltype move operator and variant", "[pipeline]") {
  const std::string source =
      "auto n = 1 + 2;\n"
      "decltype(n) m = 4;\n"
      "post(m);\n"
      "void TakeText() { auto text = \"hi\";\n"
      "const auto taken = move(text);\n"
      "post(taken);\n"
      "post(text); } TakeText();\n"
      "def Point { number x; number y; }\n"
      "number operator+(Point a, Point b) { return a.x + b.x; }\n"
      "Point p = Point { x = 1, y = 2 };\n"
      "Point q = Point { x = 3, y = 4 };\n"
      "post(p + q);\n"
      "def Box = number | string;\n"
      "const Box box = 7;\n"
      "post(box);\n"
      "const Box word = \"ok\";\n"
      "post(word);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "4\nhi\n\n4\n7\nok\n");

  const clpp::CompileResult mismatch = clpp::Compiler{}.compile("auto n = 1;\nstring s = n;\n");
  REQUIRE_FALSE(mismatch.ok());
  REQUIRE(mismatch.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult frozen = clpp::Compiler{}.compile("const auto text = \"hi\";\nconst auto taken = move(text);\n");
  REQUIRE_FALSE(frozen.ok());
  REQUIRE(frozen.diagnostics[0].message == "cannot move");
}

TEST_CASE("native threads join and atomic fetch_add", "[pipeline]") {
  const std::string source =
      "number work(number x) { return x; }\n"
      "const auto a = thread work(20);\n"
      "const auto b = thread work(22);\n"
      "post(join(a) + join(b));\n"
      "atomic n = 0;\n"
      "const auto gate = mutex();\n"
      "number inc(number id, number mid) {\n"
      "  lock(mid);\n"
      "  fetch_add(id, 1);\n"
      "  unlock(mid);\n"
      "  return 1;\n"
      "}\n"
      "const auto c = thread inc(n, gate);\n"
      "const auto d = thread inc(n, gate);\n"
      "join(c);\n"
      "join(d);\n"
      "post(atomic_load(n));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "42\n2\n");
}

TEST_CASE("list sort find and extern strlen", "[pipeline]") {
  const std::string source =
      "extern number strlen(string s);\n"
      "const auto v = list(3, 1, 2);\n"
      "const auto s = sort(v);\n"
      "post(s[0]);\n"
      "post(s[1]);\n"
      "post(s[2]);\n"
      "post(find(s, 2));\n"
      "post(strlen(\"CL++\"));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "1\n2\n3\n1\n4\n");
}

TEST_CASE("gradual modes unions and number add", "[pipeline]") {
  const std::string strict_source =
      "pragma strict;\n"
      "def State = \"Idle\" | \"Running\";\n"
      "def Same = number & number;\n"
      "const State state = \"Idle\";\n"
      "const Same n = 4;\n"
      "const number x = 2;\n"
      "const number y = 3;\n"
      "if (state == \"Idle\") { post(n); } else { post(0); }\n"
      "post(x + y);\n";
  const clpp::CompileResult strict = clpp::Compiler{}.compile(strict_source);
  REQUIRE(strict.ok());
  const clpp::RegChunk lowered = clpp::lower_registers(strict.chunk);
  bool specialized = false;
  for (const std::uint32_t word : lowered.code) {
    if (static_cast<clpp::RegOp>(clpp::reg_op(word)) == clpp::RegOp::IntAdd) {
      specialized = true;
    }
  }
  REQUIRE(specialized);
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(strict.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "4\n5\n");

  const clpp::CompileResult outside = clpp::Compiler{}.compile(
      "pragma strict;\ndef State = \"Idle\" | \"Running\";\nconst State state = \"Dead\";\n");
  REQUIRE_FALSE(outside.ok());
  REQUIRE(outside.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult sleeping = clpp::Compiler{}.compile(
      "pragma strict;\ndef State = \"Idle\" | \"Running\";\nconst State state = \"Idle\";\nif (state == \"Sleeping\") { post(0); }\n");
  REQUIRE_FALSE(sleeping.ok());
  REQUIRE(sleeping.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult crossed = clpp::Compiler{}.compile("def Bad = number & string;\nconst Bad y = 1;\n");
  REQUIRE_FALSE(crossed.ok());
  REQUIRE(crossed.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult nocheck = clpp::Compiler{}.compile("pragma nocheck;\nnumber x = \"hi\";\npost(1);\n");
  REQUIRE_FALSE(nocheck.ok());
  REQUIRE(nocheck.diagnostics.front().message == "unknown pragma mode");
  REQUIRE(nocheck.chunk.code.empty());
}

TEST_CASE("struct inheritance dispatches the dynamic method", "[pipeline]") {
  const std::string source =
      "def Animal {\n"
      "  number age;\n"
      "  number speak() { return @this.age; }\n"
      "}\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "  number speak() { return @this.bones; }\n"
      "}\n"
      "def Puppy : Dog {\n"
      "  number toys;\n"
      "  number speak() { return @this.toys; }\n"
      "}\n"
      "def Counter {\n"
      "  number add(number x) { return x; }\n"
      "}\n"
      "def Plus : Counter {\n"
      "  number add(number x) { return x + 1; }\n"
      "}\n"
      "Dog pet = Dog { age = 4, bones = 9 };\n"
      "post(pet.age);\n"
      "post(pet.bones);\n"
      "post(pet.speak());\n"
      "Animal view = pet;\n"
      "post(view.speak());\n"
      "post(view.age);\n"
      "Puppy pup = Puppy { age = 1, bones = 2, toys = 8 };\n"
      "Animal any = pup;\n"
      "post(any.speak());\n"
      "Plus extra = Plus {  };\n"
      "Counter base = extra;\n"
      "post(base.add(10));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "4\n9\n9\n9\n4\n8\n11\n");

  const clpp::CompileResult missing = clpp::Compiler{}.compile("def Dog : Missing { number bones; }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "unknown type");

  const clpp::CompileResult hidden =
      clpp::Compiler{}.compile("def Animal { number age; }\ndef Dog : Animal { number age; }\n");
  REQUIRE_FALSE(hidden.ok());
  REQUIRE(hidden.diagnostics[0].message == "already declared");

  const clpp::CompileResult bones = clpp::Compiler{}.compile(
      "def Animal { number age; }\n"
      "def Dog : Animal { number bones; }\n"
      "Animal view = Dog { age = 1, bones = 2 };\n"
      "post(view.bones);\n");
  REQUIRE_FALSE(bones.ok());
  REQUIRE(bones.diagnostics[0].message == "unknown field");

  const clpp::CompileResult shadow = clpp::Compiler{}.compile(
      "const auto x = 1;\nif (1) { const auto x = 2; post(x); }\npost(x);\n");
  REQUIRE(shadow.ok());
  std::ostringstream shadow_out;
  clpp::VirtualMachine shadow_vm;
  shadow_vm.set_output(shadow_out);
  shadow_vm.load(shadow.chunk);
  REQUIRE(shadow_vm.run());
  REQUIRE(shadow_out.str() == "2\n1\n");

  const clpp::CompileResult diamond = clpp::Compiler{}.compile(
      "def A { number health; }\n"
      "def B : A { number armor; }\n"
      "def C : A { number speed; }\n"
      "def D : B, C { number extra; }\n"
      "D value = D { health = 1, armor = 2, health = 3, speed = 4 };\n"
      "post(value.health);\n"
      "post(value.armor);\n"
      "post(value.speed);\n"
      "post(value.extra);\n");
  REQUIRE_FALSE(diamond.ok());
  REQUIRE(diamond.diagnostics[0].message == "inheritance permits only one storage base");

  const clpp::CompileResult clash = clpp::Compiler{}.compile(
      "def Left { number x; }\ndef Right { number x; }\ndef Both : Left, Right { number y; }\n");
  REQUIRE_FALSE(clash.ok());
  REQUIRE(clash.diagnostics[0].message == "inheritance permits only one storage base");

  const clpp::CompileResult remembered = clpp::Compiler{}.compile(
      "def Animal {\n"
      "  number age;\n"
      "  number speak() { return @this.age; }\n"
      "}\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "  number speak() { return @this.bones; }\n"
      "}\n"
      "const auto pet = Dog { age = 4, bones = 9 };\n"
      "const auto copy = pet;\n"
      "post(copy.speak());\n"
      "post(copy.age);\n");
  REQUIRE(remembered.ok());
  std::ostringstream kept;
  clpp::VirtualMachine remembered_vm;
  remembered_vm.set_output(kept);
  remembered_vm.load(remembered.chunk);
  REQUIRE(remembered_vm.run());
  REQUIRE(kept.str() == "9\n4\n");
}

TEST_CASE("coroutines schedule and isolated actors", "[pipeline]") {
  const std::string source =
      "number step() {\n"
      "  coroutine.yield(1);\n"
      "  return 2;\n"
      "}\n"
      "void later() { post(7); }\n"
      "number twice(number n) { return n + n; }\n"
      "const auto co = coroutine.create(step);\n"
      "post(coroutine.resume(co));\n"
      "post(coroutine.resume(co));\n"
      "task.defer(later);\n"
      "post(1);\n"
      "post(actor(twice, 21));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "1\n2\n1\n42\n7\n");

  const clpp::CompileResult blocked = clpp::Compiler{}.compile(
      "number probe(number n) { return fs::size(\"missing\"); }\npost(actor(probe, 1));\n");
  REQUIRE(blocked.ok());
  clpp::VirtualMachine sandbox;
  sandbox.load(blocked.chunk);
  REQUIRE_FALSE(sandbox.run());
  REQUIRE(sandbox.error() == "sandbox");
}

TEST_CASE("missing initializer stack overflow and crlf", "[pipeline]") {
  const clpp::CompileResult incomplete = clpp::Compiler{}.compile("const auto a =\nnumber x = \"hi\";\n");
  REQUIRE_FALSE(incomplete.ok());
  bool expected = false;
  bool mismatch = false;
  for (const clpp::Diagnostic& diagnostic : incomplete.diagnostics) {
    expected = expected || diagnostic.message == "expected expression";
    mismatch = mismatch || diagnostic.message == "type mismatch";
  }
  REQUIRE(expected);
  REQUIRE(mismatch);

  const clpp::CompileResult crlf = clpp::Compiler{}.compile("post(1);\r\npost(2);\n");
  REQUIRE(crlf.ok());
  std::ostringstream lines;
  clpp::VirtualMachine line_vm;
  line_vm.set_output(lines);
  line_vm.load(crlf.chunk);
  REQUIRE(line_vm.run());
  REQUIRE(lines.str() == "1\n2\n");

  const clpp::CompileResult recursive = clpp::Compiler{}.compile("void dive() { dive(); }\ndive();\n");
  REQUIRE(recursive.ok());
  clpp::VirtualMachine deep;
  deep.set_max_call_depth(512);
  deep.load(recursive.chunk);
  REQUIRE_FALSE(deep.run());
  REQUIRE(deep.error() == "stack overflow");
}

TEST_CASE("observable assignment runs OnChange", "[pipeline]") {
  const std::string source =
      "observable number coins = 100;\n"
      "coins.OnChange(func (number newValue) -> void { post(\"now \" .: newValue); });\n"
      "coins = 50;\n"
      "post(coins);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "now 50\n50\n");
}

TEST_CASE("axiom number library", "[pipeline]") {
  const char* source = R"cl(
link @clpp.Axion as Axiom;
post(Axiom.Clamp(12, 0, 10));
post(Axiom.Clamp(-1, 0, 10));
post(Axiom.Clamp(3, 0, 10));
post(Axiom.Map(50, 0, 100, 0, 1));
post(Axiom.Wrap(370, 0, 360));
post(Axiom.Sign(-8));
post(Axiom.Sign(0));
post(Axiom.Round(1.55));
post(Axiom.RoundDigits(1.234, 2));
post(Axiom.Snap(13, 5));
post(Axiom.Snap(13, 0));
post(Axiom.PingPong(0.5, 1));
post(Axiom.PingPong(1.5, 1));
post(Axiom.Saturate(1.4));
post(Axiom.Fract(3.25));
post(Axiom.InvLerp(10, 20, 15));
post(Axiom.Approach(0, 10, 3));
post(Axiom.Approach(9, 10, 3));
post(Axiom.IsFinite(1));
post(Axiom.Abs(-3));
post(Axiom.Min(3, 8));
post(Axiom.Max(3, 8));
post(Axiom.Pow(2, 10));
post(Axiom.Sqrt(9));
post(Axiom.Cbrt(-8));
post(Axiom.Hypot(3, 4));
post(Axiom.Log(1));
post(Axiom.Exp(0));
post(Axiom.Smoothstep(0, 1, 0.5));
post(Axiom.Smoothstep(0, 1, 0.25));
post(Axiom.Smootherstep(0, 1, 0.5));
post(Axiom.Gcd(12, 8));
post(Axiom.Lcm(12, 8));
post(Axiom.IsEven(4));
post(Axiom.IsOdd(4));
post(Axiom.Factorial(5));
post(Axiom.Scale(100, 0));
post(Axiom.Scale(100, 49));
post(Axiom.Lerp(0, 10, 0.25));
post(Axiom.Lerp(0, 10, 2));
post(Axiom.LerpClamped(0, 10, 2));
post(Axiom.LerpVector3(Vector3(0, 0, 0), Vector3(1, 1, 1), 0.5).x);
post(Axiom.Inverse(10, 20, 15));
post(Axiom.Project(Vector3(2, 0, 0), Vector3(1, 0, 0)).x);
post(Axiom.Reject(Vector3(1, 1, 0), Vector3(1, 0, 0)).y);
post(Axiom.Reflect(Vector3(1, -1, 0), Vector3(0, 1, 0)).y);
post(Axiom.Distance(Vector3(0, 0, 0), Vector3(3, 4, 0)));
post(Axiom.Distance2(Vector2(0, 0), Vector2(3, 4)));
post(Axiom.OutQuad(0.5));
post(Axiom.InOutSine(0.5));
post(Axiom.Linear(2));
post(Axiom.InQuad(0.5));
post(Axiom.RoundDigits(Axiom.InSine(1), 6));
post(Axiom.RoundDigits(Axiom.OutBounce(1), 4));
post(Axiom.CubicBezier(0, Vector3(1, 0, 0), Vector3(2, 0, 0), Vector3(3, 0, 0), Vector3(4, 0, 0)).x);
post(Axiom.Hover(0, Vector3(1, 2, 3), Vector3(0, 0, -1)).y);
post(Axiom.AabbContains(Vector3(0, 0, 0), Vector3(-1, -1, -1), Vector3(1, 1, 1)));
post(Axiom.SphereContains(Vector3(0, 0, 0), Vector3(0, 0, 0), 1));
post(Axiom.RayPlane(Vector3(0, 0, 0), Vector3(0, -1, 0), Vector3(0, -1, 0), Vector3(0, 1, 0)).y);
post(Axiom.Barycentric(Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0)).x);
post(Axiom.ClosestPointOnSegment(Vector3(0.5, 0, 0), Vector3(0, 0, 0), Vector3(1, 0, 0)).x);
post(Axiom.Deg(Axiom.Rad(90)));
post(Axiom.RoundDigits(Axiom.Atan2(1, 0), 4));
post(Axiom.Sin(0));
post(Axiom.Cos(0));
post(Axiom.Sum(list(2, 4, 6)));
post(Axiom.Average(list(2, 4, 6)));
post(Axiom.Weighted(list()));
post(Axiom.Value1(10) == Axiom.Value1(10));
post(Axiom.FromHSV(0, 1, 1).x);
post(Axiom.FromHSV(0, 1, 1).y);
post(Axiom.IsFinite(Axiom.Gaussian()));
post(Axiom.LookAt(Vector3(0, 0, 0), Vector3(0, 0, -4)).z);
post(Axiom.Flat(Vector3(1, 1, 0)).x);
post(Axiom.Slerp(Vector3(1, 0, 0), Vector3(1, 0, 0), 0.5).x);
post(Axiom.Orthonormal(Vector3(0, 0, -2)).z);
)cl";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  if (!result.ok()) {
    std::string notes;
    for (const clpp::Diagnostic& diagnostic : result.diagnostics) {
      notes += diagnostic.message;
      notes += "\n";
    }
    INFO(notes);
  }
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  const bool ran = vm.run();
  INFO(vm.error());
  REQUIRE(ran);
  REQUIRE(out.str() ==
          "10\n0\n3\n0.5\n10\n-1\n0\n2\n1.23\n15\n13\n0.5\n0.5\n1\n0.25\n0.5\n3\n10\ntrue\n3\n3\n8\n1024\n3\n-2\n5\n0\n1\n0.5\n0.15625\n0.5\n4\n24\ntrue\nfalse\n120\n2\n100\n2.5\n20\n10\n0.5\n0.5\n2\n1\n1\n5\n5\n0.75\n0.5\n1\n0.25\n1\n1\n1\n2\ntrue\ntrue\n-1\n1\n0.5\n90\n1.5708\n0\n1\n12\n4\n0\ntrue\n1\n0\ntrue\n-1\n1\n1\n-1\n");

  const clpp::CompileResult libs = clpp::Compiler{}.compile("link @clpp.libs.axiom as Axiom;\npost(Axiom.Abs(-2));\n");
  REQUIRE(libs.ok());
  std::ostringstream libs_out;
  clpp::VirtualMachine libs_vm;
  libs_vm.set_output(libs_out);
  libs_vm.load(libs.chunk);
  REQUIRE(libs_vm.run());
  REQUIRE(libs_out.str() == "2\n");

  const clpp::CompileResult utils =
      clpp::Compiler{}.compile("link @clpp.Axion as MathUtils;\npost(MathUtils.Lerp(0, 10, 0.25));\n");
  REQUIRE(utils.ok());
  std::ostringstream utils_out;
  clpp::VirtualMachine utils_vm;
  utils_vm.set_output(utils_out);
  utils_vm.load(utils.chunk);
  REQUIRE(utils_vm.run());
  REQUIRE(utils_out.str() == "2.5\n");
}

TEST_CASE("compiled chunks satisfy the structural verifier and keep a source map", "[bytecode]") {
  const char* sources[] = {"post(1);\n", "const auto x = 1;\npost(x + 2);\n", "number add(number a, number b) { return a + b; }\npost(add(1, 2));\n"};
  for (const char* source : sources) {
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE(result.ok());
    const clpp::BytecodeReport report = clpp::verify_bytecode_report(result.chunk);
    REQUIRE(report.ok());
    REQUIRE_FALSE(result.chunk.source_map.empty());
    REQUIRE(result.chunk.source_map.front().location.line >= 1);
  }
}

TEST_CASE("verifier reports a structured code and the stack checker rejects underflow alone", "[bytecode]") {
  clpp::BytecodeChunk chunk;
  chunk.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Const), 0, 0,
                static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  const clpp::BytecodeReport report = clpp::verify_bytecode_report(chunk);
  REQUIRE_FALSE(report.ok());
  REQUIRE(report.diagnostics[0].code == clpp::BytecodeError::Constant);
  REQUIRE(report.message() == "register constant");

  clpp::BytecodeChunk underflow;
  underflow.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Post),
                    static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(underflow).empty());
  clpp::compiler::bytecode::DecodedChunk decoded;
  REQUIRE(clpp::compiler::bytecode::decode(underflow.code, decoded));
  const clpp::StackCheck stack = clpp::verify_stack(underflow, decoded);
  REQUIRE(stack.error == "register stack underflow");
  REQUIRE(stack.code == clpp::BytecodeError::StackUnderflow);
}

TEST_CASE("clir serializer round-trips code and rejects a mutated chunk without crashing", "[bytecode]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("post(1);\n");
  REQUIRE(result.ok());
  const std::string bytes = clpp::serialize_clir(result.chunk);
  const clpp::BytecodeChunk restored = clpp::deserialize_clir(bytes);
  REQUIRE(restored.version == result.chunk.version);
  REQUIRE(restored.code == result.chunk.code);
  REQUIRE(restored.entry == result.chunk.entry);

  clpp::BytecodeChunk mutated = result.chunk;
  for (std::size_t index = 0; index < mutated.code.size(); ++index) {
    mutated.code[index] = static_cast<std::uint8_t>(mutated.code[index] ^ 0xA5);
    REQUIRE_NOTHROW(clpp::verify_bytecode(mutated));
    mutated.code[index] = result.chunk.code[index];
  }
}

TEST_CASE("VM executes host URL parsing without another backend", "[pipeline][host]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("link @clpp.http as Http;\npost(Http.host(\"http://h.test/a\"));\n");
  REQUIRE(result.ok());
  std::ostringstream output;
  clpp::VirtualMachine vm;
  vm.set_output(output);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(output.str() == "h.test\n");
}

TEST_CASE("CLIR round trip executes literals functions and virtual methods", "[bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(
      "def Player { number Health = 75; number Read() { return @this.Health; } }\n"
      "number Add(number a, number b) { return a + b; } Player p = Player {};\n"
      "post(p.Read()); post(Add(2, 3)); post(2.5); post(\"hello\"); post(null);\n");
  REQUIRE(compiled.ok());
  REQUIRE_FALSE(compiled.chunk.vtables.empty());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes);
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  REQUIRE(clpp::verify_bytecode(restored).empty());
  REQUIRE(restored.constants.size() == compiled.chunk.constants.size());
  REQUIRE(restored.vtables == compiled.chunk.vtables);
  REQUIRE(restored.source_map.size() == compiled.chunk.source_map.size());
  for (std::size_t i = 0; i < restored.source_map.size(); ++i) {
    REQUIRE(restored.source_map[i].pc == compiled.chunk.source_map[i].pc);
    REQUIRE(restored.source_map[i].location.line == compiled.chunk.source_map[i].location.line);
    REQUIRE(restored.source_map[i].location.column == compiled.chunk.source_map[i].location.column);
  }
  std::ostringstream output;
  clpp::VirtualMachine vm;
  vm.set_output(output);
  vm.load(restored);
  REQUIRE(vm.run());
  REQUIRE(output.str() == "75\n5\n2.5\nhello\nnull\n");
  REQUIRE(clpp::serialize_clir(restored) == bytes);
}

TEST_CASE("CLIR preserves interface method slots and rejects invalid vtable targets", "[interfaces][bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    abstract def Name { abstract string ReadName(); }
    abstract def Score { abstract number ReadScore(); }
    def Player : Name, Score {
      string ReadName() { return "Ada"; }
      number ReadScore() { return 31; }
    }
    Name name = Player {}; Score score = Player {};
    post(name.ReadName()); post(score.ReadScore());
  )");
  REQUIRE(compiled.ok());
  auto restored = clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk));
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  REQUIRE(restored.vtables == compiled.chunk.vtables);
  REQUIRE(clpp::verify_bytecode(restored).empty());
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "Ada\n31\n");
  REQUIRE_FALSE(restored.vtables.empty()); REQUIRE_FALSE(restored.vtables[0].empty());
  restored.vtables[0][0] = static_cast<std::uint16_t>(restored.functions.size());
  CHECK_FALSE(clpp::verify_bytecode(restored).empty());
}

TEST_CASE("CLIR preserves event Wait suspension and typed heterogeneous payloads", "[event-wait][bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    async number Read(signal<number,string> event) {
      auto payload = event::Wait(); post(payload[1]); return payload[0];
    }
    auto event = signal<number,string>(); auto work = spawn Read(event);
    event::Fire(37,"ready"); post(await work);
  )");
  REQUIRE(compiled.ok());
  const auto restored = clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk));
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  REQUIRE(clpp::verify_bytecode(restored).empty());
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "ready\n37\n");
  clpp::compiler::bytecode::DecodedChunk decoded;
  REQUIRE(clpp::compiler::bytecode::decode(compiled.chunk.code, decoded));
  bool found_wait = false;
  for (const auto& instruction : decoded.instructions) {
    if (instruction.op != clpp::compiler::Opcode::Event || (instruction.imm & 255) != 11) continue;
    auto malformed = compiled.chunk; malformed.code[instruction.pc + 2] = 2;
    CHECK_FALSE(clpp::verify_bytecode(malformed).empty()); found_wait = true;
  }
  CHECK(found_wait);
}

TEST_CASE("execution errors cancel Wait captures and release locks before joining producers", "[event-wait][shared-heap]") {
  clpp::collect_shared_heap(); const auto baseline = clpp::shared_heap_stats().objects;
  const auto compiled = clpp::Compiler{}.compile(R"(
    async number Listen(signal<number> event, number gate) {
      array<number> retained = array<number>(7,9);
      lock(gate); number value = event::Wait(); unlock(gate); return value + retained[0];
    }
    number Producer(number gate) { lock(gate); unlock(gate); return 1; }
    auto event = signal<number>(); number gate = mutex();
    auto waiting = spawn Listen(event, gate);
    auto producer = thread Producer(gate);
    report("stop execution");
  )");
  REQUIRE(compiled.ok());
  for (int iteration = 0; iteration < 12; ++iteration) {
    clpp::VirtualMachine vm; vm.load(compiled.chunk);
    CHECK_FALSE(vm.run()); CHECK(vm.error() == "stop execution");
  }
  clpp::collect_shared_heap(); CHECK(clpp::shared_heap_stats().objects == baseline);
}

TEST_CASE("CLIR preserves async callback scheduling and captured task arguments", "[async-callback][bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    def Getter = () -> task<number>;
    Getter Make(number value) { return async func() -> number { return value; }; }
    auto callback = Make(42); post(await spawn callback());
    async string Echo(string text) { return text; }
    auto echo = Echo; post(await echo("typed"));
  )");
  for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes);
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  REQUIRE(restored.functions.size() == compiled.chunk.functions.size());
  std::size_t asynchronous = 0;
  for (std::size_t index = 0; index < restored.functions.size(); ++index) {
    CHECK(restored.functions[index].is_async == compiled.chunk.functions[index].is_async);
    if (restored.functions[index].is_async) ++asynchronous;
  }
  CHECK(asynchronous == 2);
  std::ostringstream output;
  clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "42\ntyped\n");
  CHECK(clpp::serialize_clir(restored) == bytes);
  auto old = bytes; old[6] = 5; old[7] = 0;
  CHECK(clpp::deserialize_clir(old).version == 0);
  std::size_t suffix = 2 + 4 + compiled.chunk.source_map.size() * 10;
  for (const auto& table : compiled.chunk.vtables) suffix += 2 + table.size() * 2;
  const auto first_function = bytes.size() - suffix - compiled.chunk.functions.size() * 8;
  auto invalid_flags = bytes; invalid_flags[first_function + 6] = 2;
  CHECK(clpp::deserialize_clir(invalid_flags).version == 0);
  for (std::size_t size = 0; size < bytes.size(); ++size)
    CHECK(clpp::deserialize_clir(std::string_view(bytes).substr(0, size)).version == 0);
}

TEST_CASE("CLIR preserves bound receiver methods and async virtual dispatch", "[method-callback][bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    def Base { number Value; async number Read() { return @this.Value; } }
    def Child : Base { override async number Read() { return @this.Value + 1; } }
    Base receiver = Child { Value = 41 }; auto callback = receiver.Read;
    receiver = Base { Value = 0 };
    post(await spawn callback()); post(await receiver.Read());
  )");
  for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes);
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "42\n0\n");
  CHECK(clpp::serialize_clir(restored) == bytes);
  auto old = bytes; old[6] = 6; old[7] = 0; CHECK(clpp::deserialize_clir(old).version == 0);
  auto invalid = compiled.chunk;
  bool found = false;
  clpp::compiler::bytecode::DecodedChunk decoded;
  REQUIRE(clpp::compiler::bytecode::decode(invalid.code, decoded));
  for (const auto& instruction : decoded.instructions) {
    if (instruction.op == clpp::compiler::Opcode::MakeBoundMethod) {
      invalid.code[instruction.pc + 1] = 255; invalid.code[instruction.pc + 2] = 255; found = true; break;
    }
  }
  REQUIRE(found); CHECK(clpp::verify_bytecode(invalid) == "register method slot");
}

TEST_CASE("compiler and IDE analysis retain separate concurrent and nested contexts", "[compiler-reentrancy]") {
  std::atomic<int> failures{0}; std::vector<std::thread> workers;
  for (int worker = 0; worker < 6; ++worker) workers.emplace_back([worker, &failures] {
    const auto type = worker % 2 ? std::string("string") : std::string("number");
    const auto value = worker % 2 ? std::string("\"typed\"") : std::string("42");
    for (int index = 0; index < 40; ++index) {
      const auto source = "def Transform<T> = (T)->T; def Box<T> { array<T> Items = array<T>(); Transform<T> Apply = func(value)->T { return value; }; } auto box = Box<" + type + "> {}; post(box.Apply(" + value + "));";
      const auto compiled = clpp::Compiler{}.compile(source);
      if (!compiled.ok() || !clpp::analyze_program(source, {}).ok()) ++failures;
      const clpp::ModuleLoader loader = [&](std::string_view) -> std::optional<std::string> {
        if (!clpp::Compiler{}.compile(source).ok()) ++failures;
        return "export def Item { " + type + " Value; }";
      };
      if (!clpp::Compiler{}.compile("link \"./m.clp\" as M; auto item = M.Item { Value = " + value + " }; post(item.Value);", loader).ok()) ++failures;
    }
  });
  for (auto& worker : workers) worker.join();
  CHECK(failures.load() == 0);
}

TEST_CASE("CLIR preserves specialized generic defaults and independent instances", "[generic-default][bytecode]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    def Transform<T> = (T)->T;
    template <typename T> T Identity(T value) { return value; }
    def Base<T> { array<T> Items = array<T>(); Transform<T> Apply = Identity<T>; signal<T> Changed = signal<T>(); }
    def Derived<U> : Base<U> {}
    auto first = Derived<number> {}; auto second = Derived<number> {};
    push(first.Items, 7); post(len(second.Items)); post(first.Apply(9));
    first.Changed::Connect(func(value) -> void { post(value); }); first.Changed::Fire(11); second.Changed::Fire(13);
  )");
  for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(compiled.ok()); const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes); REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  clpp::VirtualMachine vm; std::ostringstream output; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "0\n9\n11\n"); CHECK(clpp::serialize_clir(restored) == bytes);
}

TEST_CASE("root execution releases native resources on success failure and worker completion", "[resource-lifetime]") {
  const char* previous = std::getenv("CLPP_HEADLESS");
  const std::string prior = previous ? previous : "";
  const bool had_previous = previous != nullptr;
  struct Restore {
    std::string prior; bool present;
    ~Restore() {
#ifdef _WIN32
      _putenv_s("CLPP_HEADLESS", present ? prior.c_str() : "");
#else
      if (present) setenv("CLPP_HEADLESS", prior.c_str(), 1); else unsetenv("CLPP_HEADLESS");
#endif
    }
  } restore{prior, had_previous};
  set_test_env("CLPP_HEADLESS", "10");
  const auto check_clean = [&] {
    const auto& canvas = clpp::stdlib::host::canvas();
    CHECK(canvas.width == 0); CHECK(canvas.height == 0); CHECK(canvas.pixels.capacity() == 0);
    CHECK_FALSE(clpp::stdlib::text::choice().pixel);
    CHECK(clpp::stdlib::host::ui_style().font == 15);
    CHECK(clpp::stdlib::host::input().frame == 0);
    int width = 0, height = 0; CHECK_FALSE(clpp::stdlib::host::image_dimensions(1, width, height));
    clpp::Value sound = clpp::Value::integer_of(1), out; std::string error;
    const auto native = clpp::stdlib::host_native("audio::Duration", 1);
    REQUIRE(native >= 2000);
    CHECK_FALSE(clpp::stdlib::host_apply(static_cast<std::uint8_t>(native - 2000), &sound, 1, out, error));
    CHECK(error.find("invalid sound handle") != std::string::npos);
    const clpp::Value query[] = {clpp::Value::string_of("resource-state"), clpp::Value::integer_of(2)};
    const auto gui = clpp::stdlib::host_native("gui::Get", 2); REQUIRE(gui >= 2000);
    REQUIRE(clpp::stdlib::host_apply(static_cast<std::uint8_t>(gui - 2000), query, 2, out, error));
    CHECK(out.text.empty());
  };
  for (const auto& ending : {std::string("post(\"done\");"), std::string("report(\"resource failure\");"),
                            std::string("void Work() { post(Audio.Duration(sound)); } auto worker = thread Work();"),
                            std::string("void Work() { post(Audio.Duration(sound)); report(\"resource failure\"); } auto worker = thread Work();")}) {
    const auto compiled = clpp::Compiler{}.compile(R"(
      link @clpp.window as Window; link @clpp.gfx as Gfx; link @clpp.audio as Audio;
      link @clpp.ui as Ui; link @clpp.gui as Gui;
      Window.Open("resource test", 16, 16); Window.Frame();
      auto image = Gfx.Capture(0, 0, 4, 4); Ui.FontSize(24); Gui.SetText("resource-state", "retained");
      Gfx.Font("pixel"); auto sound = Audio.Tone(440, 0.01, "sine"); auto voice = Audio.Play(sound, 1, 0, 1, true);
      signal<> changed; changed::Once(func() -> void { post(Audio.Duration(sound)); }); changed::Fire();
    )" + ending);
    for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
    REQUIRE(compiled.ok());
    clpp::VirtualMachine vm; std::ostringstream output; vm.set_output(output); vm.load(compiled.chunk);
    const bool failure = ending.find("report") != std::string::npos;
    CHECK(vm.run() == !failure);
    if (failure) CHECK(vm.error() == "resource failure"); else CHECK_FALSE(output.str().empty());
    check_clean();
    output.str(""); CHECK(vm.run() == !failure); check_clean();
  }
}

TEST_CASE("execution boundaries reset cached images and native handles", "[resource-lifetime]") {
  const auto path = std::filesystem::temp_directory_path() / "clpp-resource-lifetime-cache.bmp";
  struct Remove { std::filesystem::path path; ~Remove() { std::error_code ignored; std::filesystem::remove(path, ignored); } } remove{path};
  for (const auto width : {3, 7, 11}) {
    const auto compiled = clpp::Compiler{}.compile(
      "link @clpp.gfx as Gfx; Gfx.Canvas(" + std::to_string(width) + ", 4); Gfx.Save(\"" + path.generic_string() + "\");");
    REQUIRE(compiled.ok()); clpp::VirtualMachine vm; vm.load(compiled.chunk); REQUIRE(vm.run());
    {
      clpp::stdlib::HostExecution execution;
      std::string error; int w = 0, h = 0;
      const auto handle = clpp::stdlib::host::load_image_file(path.string(), error);
      INFO(error); REQUIRE(handle == 1); REQUIRE(clpp::stdlib::host::image_dimensions(handle, w, h));
      CHECK(w == width); CHECK(h == 4);
      CHECK(clpp::stdlib::host::load_image_file(path.string(), error) == handle);
      clpp::stdlib::text::choice().family = "Arial";
      CHECK(clpp::stdlib::text::width("resources", 17) > 0);
    }
    int w = 0, h = 0; CHECK_FALSE(clpp::stdlib::host::image_dimensions(1, w, h));
    CHECK(clpp::stdlib::text::choice().family == "Segoe UI");
  }
}

TEST_CASE("observable connections survive CLIR and release callback ownership", "[observable-connection][bytecode][shared-heap]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    observable number value = 0;
    auto handle = value.OnChange(func(next) -> void { post(next); });
    value = 42; handle::Disconnect(); value = 7; post(handle.Connected);
  )");
  for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  auto bytes = clpp::serialize_clir(compiled.chunk);
  auto restored = clpp::deserialize_clir(bytes); REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "42\nfalse\n");
  bytes[6] = 8; bytes[7] = 0; CHECK(clpp::deserialize_clir(bytes).version == 0);

  auto cell = clpp::Value::cell_of(clpp::Value::integer_of(17));
  auto code = std::make_shared<clpp::BytecodeChunk>(); std::weak_ptr<const clpp::BytecodeChunk> retained = code;
  auto callback = clpp::Value::closure_of(0, {clpp::Value::cell_of(clpp::Value::integer_of(42))}, code);
  auto handle = cell.subscribe(callback, false);
  callback = clpp::Value::null_of(); code.reset(); clpp::collect_shared_heap();
  REQUIRE_FALSE(retained.expired()); handle.disconnect(); handle.disconnect(); clpp::collect_shared_heap();
  CHECK(retained.expired()); CHECK(cell.connections().empty()); CHECK(cell.field(0).integer == 17);
  for (int index = 0; index < 500; ++index) {
    auto subscription = cell.subscribe(clpp::Value::closure_of(0, {}, {}), false);
    subscription.disconnect();
  }
  CHECK(cell.connections().empty()); CHECK(cell.field_count() == 1);
}

TEST_CASE("CLIR preserves typed event subscriptions and checked operands", "[event-subscription][bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile(R"(
    signal<number> event;
    auto connection = event~>Connect(func(value) -> void { post(value); });
    event::Fire(42); event::Clear(); event::Fire(7); post(connection.Connected);
  )");
  for (const auto& diagnostic : compiled.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(compiled.ok());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes); REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == "42\nfalse\n"); CHECK(clpp::serialize_clir(restored) == bytes);
  auto old = bytes; old[6] = 7; old[7] = 0; CHECK(clpp::deserialize_clir(old).version == 0);
  auto invalid = compiled.chunk;
  clpp::compiler::bytecode::DecodedChunk decoded; REQUIRE(clpp::compiler::bytecode::decode(invalid.code, decoded));
  bool found = false;
  for (const auto& instruction : decoded.instructions) if (instruction.op == clpp::compiler::Opcode::Event) {
    invalid.code[instruction.pc + 1] = 255; found = true; break;
  }
  REQUIRE(found); CHECK(clpp::verify_bytecode(invalid) == "register event operand");
}

TEST_CASE("managed event cleanup releases callback owners and is reusable", "[event-subscription][shared-heap]") {
  auto signal = clpp::Value::signal_of();
  auto code = std::make_shared<clpp::BytecodeChunk>(); std::weak_ptr<const clpp::BytecodeChunk> retained = code;
  auto callback = clpp::Value::closure_of(0, {clpp::Value::cell_of(clpp::Value::struct_of({clpp::Value::integer_of(42)}))}, code);
  auto connection = signal.subscribe(callback, false, true);
  callback = clpp::Value::null_of(); code.reset(); clpp::collect_shared_heap();
  CHECK_FALSE(retained.expired()); REQUIRE(connection.connected());
  signal.clear_managed(); signal.clear_managed(); clpp::collect_shared_heap();
  CHECK_FALSE(connection.connected()); CHECK(retained.expired()); CHECK(signal.connections().empty());
  auto regular = signal.subscribe(clpp::Value::closure_of(0, {}, {}), false);
  signal.clear_managed(); CHECK(regular.connected()); regular.disconnect(); regular.disconnect();
  CHECK(signal.connections().empty());
  for (int index = 0; index < 300; ++index) {
    auto value = signal.subscribe(clpp::Value::closure_of(0, {}, {}), false, true);
    signal.clear_managed(); REQUIRE_FALSE(value.connected());
  }
  CHECK(signal.connections().empty());
}

TEST_CASE("repeated event subscription cleanup bounds live heap pressure", "[event-subscription][shared-heap]") {
  clpp::collect_shared_heap(); const auto baseline = clpp::shared_heap_stats().objects;
  struct Probe : std::stringbuf {
    std::size_t peak{0};
    std::streamsize xsputn(const char* text, std::streamsize size) override {
      peak = std::max(peak, clpp::shared_heap_stats().objects);
      return std::stringbuf::xsputn(text, size);
    }
  } probe;
  const auto compiled = clpp::Compiler{}.compile(R"(
    def Receiver { void Changed(number value) {} }
    auto receiver = Receiver {}; auto callback = receiver.Changed; signal<number> event;
    for (number index = 0; index < 2000; index += 1) { event~>Connect(callback); event::Clear(); }
    post("probe");
  )");
  REQUIRE(compiled.ok()); std::ostream output(&probe); clpp::VirtualMachine vm; vm.set_output(output); vm.load(compiled.chunk);
  REQUIRE(vm.run()); CHECK(probe.str() == "probe\n"); CHECK(probe.peak < baseline + 100);
}

TEST_CASE("CLIR preserves binary strings floating bits and vector dimensions", "[bytecode][clir-io]") {
  auto compiled = clpp::Compiler{}.compile("post(1);");
  REQUIRE(compiled.ok());
  const std::string binary("a\0b\xC3\xA9", 5);
  compiled.chunk.constants.push_back(clpp::Value::string_of(binary));
  compiled.chunk.constants.push_back(clpp::Value::number_of(-0.0));
  compiled.chunk.constants.push_back(clpp::Value::vector_of(1, 2, 3, 4, 4));
  compiled.chunk.constants.push_back(clpp::Value::null_of());
  const auto restored = clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk));
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  const auto end = restored.constants.size();
  REQUIRE(restored.constants[end - 4].text == binary);
  REQUIRE(std::bit_cast<std::uint64_t>(restored.constants[end - 3].number) == std::bit_cast<std::uint64_t>(-0.0));
  REQUIRE(restored.constants[end - 2].dims == 4);
  REQUIRE(restored.constants[end - 2].number == 1);
  REQUIRE(restored.constants[end - 2].y == 2);
  REQUIRE(restored.constants[end - 2].z == 3);
  REQUIRE(restored.constants[end - 2].w == 4);
  REQUIRE(restored.constants[end - 1].is_null());
}

TEST_CASE("CLIR rejects every truncation unknown revisions and trailing bytes", "[bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile("post(\"abc\"); post(null);");
  REQUIRE(compiled.ok());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  for (std::size_t size = 0; size < bytes.size(); ++size) {
    INFO(size);
    const auto invalid = clpp::deserialize_clir(std::string_view(bytes).substr(0, size));
    REQUIRE(invalid.version == 0);
    REQUIRE(invalid.code.empty());
    REQUIRE(invalid.constants.empty());
    REQUIRE(invalid.functions.empty());
    REQUIRE(invalid.vtables.empty());
    REQUIRE(invalid.source_map.empty());
  }
  for (const std::size_t offset : {0u, 4u, 6u}) {
    auto mutated = bytes;
    mutated[offset] = static_cast<char>(0xFF);
    REQUIRE(clpp::deserialize_clir(mutated).version == 0);
  }
  REQUIRE(clpp::deserialize_clir(bytes + "junk").version == 0);
  auto huge_code = bytes;
  for (std::size_t i = 12; i < 16; ++i) huge_code[i] = static_cast<char>(0xFF);
  REQUIRE(clpp::deserialize_clir(huge_code).version == 0);
  auto unknown_literal = bytes;
  unknown_literal[16 + compiled.chunk.code.size() + 2] = static_cast<char>(0xFF);
  REQUIRE(clpp::deserialize_clir(unknown_literal).version == 0);
  auto invalid_location = bytes;
  for (std::size_t i = invalid_location.size() - 8; i < invalid_location.size() - 4; ++i) invalid_location[i] = 0;
  REQUIRE(clpp::deserialize_clir(invalid_location).version == 0);
}

TEST_CASE("CLIR rejects runtime heap constants and malformed source positions", "[bytecode][clir-io]") {
  const auto compiled = clpp::Compiler{}.compile("post(1);");
  REQUIRE(compiled.ok());
  for (auto value : {clpp::Value::struct_of({clpp::Value::number_of(1)}),
       clpp::Value::task_of(clpp::Value::number_of(1)), clpp::Value::table_of(nullptr),
       clpp::Value::buffer_of(4), clpp::Value::vector_of(0, 0, 0, 0, 5)}) {
    auto chunk = compiled.chunk;
    chunk.constants.push_back(std::move(value));
    REQUIRE_THROWS_AS(clpp::serialize_clir(chunk), std::invalid_argument);
  }
  auto chunk = compiled.chunk;
  chunk.source_map.push_back({static_cast<std::uint16_t>(chunk.code.size()), {1, 1}});
  REQUIRE_THROWS_AS(clpp::serialize_clir(chunk), std::invalid_argument);
  clpp::BytecodeChunk underflow;
  underflow.code = {static_cast<std::uint8_t>(clpp::compiler::Opcode::Post),
                    static_cast<std::uint8_t>(clpp::compiler::Opcode::Halt)};
  REQUIRE(clpp::verify_bytecode(underflow).empty());
  REQUIRE_THROWS_AS(clpp::serialize_clir(underflow), std::invalid_argument);
}

TEST_CASE("CLIR integer literals survive round trips without floating conversion", "[bytecode][clir-io][integer-storage]") {
  const auto compiled = clpp::Compiler{}.compile("post(9007199254740993); post(-9223372036854775808);");
  REQUIRE(compiled.ok());
  const auto bytes = clpp::serialize_clir(compiled.chunk);
  const auto restored = clpp::deserialize_clir(bytes);
  REQUIRE(restored.version == clpp::kBytecodeFormatVersion);
  REQUIRE(restored.constants[0].is_integer()); REQUIRE(restored.constants[0].integer == 9007199254740993LL);
  std::ostringstream output;
  clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); REQUIRE(output.str() == "9007199254740993\n-9223372036854775808\n");
  REQUIRE(clpp::serialize_clir(restored) == bytes);
}

TEST_CASE("garbage tracing traverses deep cyclic object graphs without host recursion", "[gc][runtime-context]") {
  std::vector<std::unique_ptr<clpp::Table>> heap;
  clpp::Table* first = clpp::vm::allocate(heap);
  clpp::Table* previous = first;
  for (std::size_t index = 1; index < 30000; ++index) {
    clpp::Table* next = clpp::vm::allocate(heap);
    previous->entries.emplace_back("next", clpp::Value::table_of(next));
    previous = next;
  }
  previous->entries.emplace_back("cycle", clpp::Value::table_of(first));
  clpp::Value root = clpp::Value::task_of(clpp::Value::struct_of({clpp::Value::table_of(first)}));
  (void)clpp::vm::allocate(heap);
  clpp::vm::mark_value(root); clpp::vm::sweep(heap);
  REQUIRE(heap.size() == 30000);
  for (const auto& object : heap) object->marked = false;
  root = clpp::Value::null_of(); clpp::vm::mark_value(root); clpp::vm::sweep(heap);
  REQUIRE(heap.empty());
}

TEST_CASE("shared heap collects cycles and preserves external native roots", "[gc][shared-heap]") {
  clpp::Value anchor = clpp::Value::struct_of({clpp::Value::integer_of(99)});
  clpp::collect_shared_heap(); const auto baseline = clpp::shared_heap_stats().objects;
  {
    clpp::Value first = clpp::Value::struct_of({clpp::Value::null_of()});
    clpp::Value second = clpp::Value::struct_of({first}); first.set_field(0, second);
    REQUIRE(first.field(0).identity() == second.identity());
    REQUIRE(clpp::shared_heap_stats().objects == baseline + 2);
  }
  REQUIRE(clpp::collect_shared_heap() == 2);
  REQUIRE(clpp::shared_heap_stats().objects == baseline);
  REQUIRE(anchor.field(0).integer == 99);
  clpp::Value native_root;
  {
    auto child = clpp::Value::struct_of({clpp::Value::integer_of(42)});
    auto parent = clpp::Value::struct_of({child}); native_root = parent.field(0);
  }
  clpp::collect_shared_heap(); REQUIRE(native_root.field(0).integer == 42);
}

TEST_CASE("shared heap freezing is transitive cycle safe and alias wide", "[gc][shared-heap]") {
  auto child = clpp::Value::struct_of({clpp::Value::null_of()});
  auto parent = clpp::Value::struct_of({child}); child.set_field(0,parent);
  auto alias = child; parent.freeze();
  REQUIRE(parent.frozen()); REQUIRE(alias.frozen());
  REQUIRE_THROWS_AS(alias.set_field(0,clpp::Value::null_of()),clpp::HeapError);
  REQUIRE_THROWS_AS(alias.append_field(clpp::Value::integer_of(1)),clpp::HeapError);
  REQUIRE_THROWS_AS(parent.remove_field(0),clpp::HeapError);
  REQUIRE(alias.field(0).identity() == parent.identity());
}

TEST_CASE("shared heap traces and destroys deep graphs without recursive reclamation", "[gc][shared-heap]") {
  auto anchor = clpp::Value::struct_of({}); clpp::collect_shared_heap();
  const auto baseline = clpp::shared_heap_stats().objects;
  {
    auto current = clpp::Value::struct_of({});
    for (int index=0; index<20000; ++index) current=clpp::Value::struct_of({current});
    clpp::collect_shared_heap(); REQUIRE(current.field_count()==1);
    current.freeze(); REQUIRE(current.frozen());
  }
  clpp::collect_shared_heap(); REQUIRE(clpp::shared_heap_stats().objects == baseline);
}

TEST_CASE("native roots remain valid while another thread collects and freezes graphs", "[gc][shared-heap]") {
  auto shared = clpp::Value::struct_of({clpp::Value::integer_of(42)});
  std::atomic<bool> good{true};
  std::thread reader([shared,&good] {
    for (int i=0;i<3000;++i) { auto alias=shared; if (alias.field(0).integer!=42) good=false; }
  });
  std::thread collector([shared] {
    for (int i=0;i<500;++i) { auto garbage=clpp::Value::struct_of({clpp::Value::null_of()}); garbage.set_field(0,garbage); garbage=clpp::Value::null_of(); clpp::collect_shared_heap(); }
    shared.freeze();
  });
  reader.join(); collector.join(); REQUIRE(good.load()); REQUIRE(shared.frozen());
  REQUIRE(shared.field(0).integer==42);
}

TEST_CASE("native dictionary removal supports an output alias of the receiver", "[gc][shared-heap]") {
  auto dictionary=clpp::Value::struct_of({clpp::Value::string_of("key"),clpp::Value::integer_of(42)});
  dictionary.number=65535; auto alias=dictionary;
  REQUIRE(dictionary.dictionary_remove(clpp::Value::string_of("key"),dictionary));
  REQUIRE(dictionary.is_integer()); REQUIRE(dictionary.integer==42);
  REQUIRE(alias.field_count()==0); clpp::collect_shared_heap(); REQUIRE(alias.field_count()==0);
}


namespace {
std::string execute_callback_test(const std::string& source) {
  const auto result = clpp::Compiler{}.compile(source);
  std::string notes;
  for (const auto& diagnostic : result.diagnostics) notes += diagnostic.message + " at " + std::to_string(diagnostic.location.line) + "\n";
  INFO(notes);
  REQUIRE(result.ok());
  std::ostringstream output;
  clpp::VirtualMachine vm; vm.set_output(output); vm.load(result.chunk);
  const bool ok = vm.run(); INFO(vm.error()); REQUIRE(ok);
  return output.str();
}
}

TEST_CASE("typed callbacks store pass and invoke named functions", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Transform = (number) -> number;
    number Twice(number value) { return value * 2; }
    number Apply(Transform action, number value) { return action(value); }
    Transform selected = Twice;
    post(Apply(selected, 21));
    selected = func(number value) -> number { return value + 1; };
    post(selected(41));
    post((func(number value) -> number { return value * 3; })(14));
  )") == "42\n42\n42\n");
}

TEST_CASE("closures retain and share captured binding cells", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Counter = () -> number;
    Counter Create(number start) {
      number count = start;
      Counter next = func() -> number { count += 1; return count; };
      count += 10;
      return next;
    }
    const auto next = Create(1);
    auto alias = next;
    post(next()); post(alias()); post(next());
  )") == "12\n13\n14\n");
}

TEST_CASE("nested closures forward lexical cells and shadow parameters", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Counter = () -> number;
    def Factory = () -> Counter;
    Factory Create() {
      number count = 4;
      return func() -> Counter {
        return func() -> number { count += 1; return count; };
      };
    }
    auto factory = Create();
    auto a = factory(); auto b = factory();
    post(a()); post(b());
    number value = 99;
    auto shadow = func(number value) -> number { return value; };
    post(shadow(7)); post(value);
  )") == "5\n6\n7\n99\n");
}

TEST_CASE("callback signatures and captured const are checked", "[callbacks]") {
  for (const auto* source : {
      "def F = (number) -> number; F f = func(string x) -> number { return 1; };",
      "def F = (number) -> number; F f = func(number x) -> string { return x; };",
      "auto f = func(number x) -> number { return x; }; f();",
      "auto f = func(number x) -> number { return x; }; f(\"wrong\");",
      "const number x = 1; auto f = func() -> number { x = 2; return x; };",
      "const number x = 1; auto f = func() -> number { return x++; };",
      "auto f = func(x) -> number { return x; };",
      "number x = 1; x();"}) {
    INFO(source); CHECK_FALSE(clpp::Compiler{}.compile(source).ok());
  }
}

TEST_CASE("closures share captured cells across synchronized workers", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    number total = 0;
    number gate = mutex();
    auto work = func(number amount) -> number {
      lock(gate); total += amount; unlock(gate); return amount;
    };
    number a = thread work(20);
    number b = thread work(22);
    join(a); join(b); post(total);
  )") == "42\n");
}


TEST_CASE("callbacks in fields arrays and generic signatures retain concrete types", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Transform<T> = (T) -> T;
    def Box { Transform<number> Apply; }
    template <typename T>
    T Map(T value, Transform<T> transform) { return transform(value); }
    Transform<number> twice = func(number value) -> number { return value * 2; };
    Box box = Box { Apply = twice };
    array<Transform<number>> operations = array<Transform<number>>(twice);
    post(box.Apply(7)); post(operations[0](8)); post(Map(9, twice));
    array<number> values = array<number>();
    auto pushValue = func(number value) -> number { push(values, value); return len(values); };
    post(pushValue(3)); post(values[0]);
  )") == "14\n16\n18\n1\n3\n");
}

TEST_CASE("closures preserve distinct block bindings and callback errors", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Counter = () -> number;
    array<Counter> callbacks = array<Counter>();
    for (number i = 0; i < 3; i++) {
      number own = i;
      push(callbacks, func() -> number { return own; });
    }
    post(callbacks[0]()); post(callbacks[1]()); post(callbacks[2]());
    auto fail = func() -> number { report("callback failure"); return 0; };
    try { fail(); } catch (e) { post("caught"); }
  )") == "0\n1\n2\ncaught\n");
}

TEST_CASE("closure cells trace cycles and retain executable ownership", "[callbacks][heap]") {
  clpp::collect_shared_heap();
  const auto before = clpp::shared_heap_stats().objects;
  std::weak_ptr<const clpp::BytecodeChunk> weak_code;
  {
    auto code = std::make_shared<const clpp::BytecodeChunk>(); weak_code = code;
    auto cell = clpp::Value::cell_of(clpp::Value::null_of());
    auto closure = clpp::Value::closure_of(0, {cell}, code);
    cell.set_field(0, closure);
    code.reset(); CHECK_FALSE(weak_code.expired());
    clpp::collect_shared_heap(); CHECK(closure.field(0).field(0).identity() == closure.identity());
    closure.freeze(); CHECK_FALSE(cell.frozen());
  }
  clpp::collect_shared_heap();
  CHECK(weak_code.expired()); CHECK(clpp::shared_heap_stats().objects == before);
}


TEST_CASE("observable subscriptions retain typed closure handlers and propagate failures", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    observable number score = 0;
    number bonus = 10;
    score.OnChange(func(number value) -> void { post(value + bonus); });
    bonus = 20; score = 2;
    score.OnChange(func(number value) -> void { report("handler failure"); });
    try { score = 3; } catch (e) { post("caught"); }
    post(score);
  )") == "22\n23\ncaught\n3\n");
  CHECK_FALSE(clpp::Compiler{}.compile("observable number score = 0; score.OnChange(func(string value) -> void {});").ok());
  CHECK_FALSE(clpp::Compiler{}.compile("observable number score = 0; score.OnChange(func(number value) -> number { return value; });").ok());
}


TEST_CASE("for-in closures retain each iteration binding", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Read = () -> number;
    array<Read> reads = array<Read>();
    for (number value in 3) { push(reads, func() -> number { return value; }); }
    for (number value in array<number>(7, 8)) { push(reads, func() -> number { return value; }); }
    for (Read read in reads) { post(read()); }
  )") == "0\n1\n2\n7\n8\n");
}

TEST_CASE("imported callbacks and factory closures keep module identity", "[callbacks]") {
  const auto result = clpp::Compiler{}.compile(R"(
    link "./counter.clp" as Counter;
    auto next = Counter.Create(3);
    post(next()); post(next());
  )", [](std::string_view path) -> std::optional<std::string> {
    if (path == "counter.clp" || path == "./counter.clp") return R"(
      export def Read = () -> number;
      export Read Create(number start) {
        number value = start;
        return func() -> number { value += 1; return value; };
      }
    )";
    return std::nullopt;
  });
  for (const auto& diagnostic : result.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(result.ok());
  std::ostringstream out; clpp::VirtualMachine vm; vm.set_output(out); vm.load(result.chunk);
  REQUIRE(vm.run()); CHECK(out.str() == "4\n5\n");
}


TEST_CASE("module closure captures shadow function names and module constants", "[callbacks]") {
  const auto result = clpp::Compiler{}.compile("link \"./mod.clp\" as M; auto read = M.Create(); post(read());",
    [](std::string_view) -> std::optional<std::string> { return R"(
      const number OFFSET = 100;
      number Value() { return 200; }
      export def Read = () -> number;
      export Read Create() {
        number OFFSET = 2;
        number Value = 3;
        return func() -> number { return OFFSET + Value; };
      }
    )"; });
  for (const auto& diagnostic : result.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(result.ok()); std::ostringstream out; clpp::VirtualMachine vm; vm.set_output(out); vm.load(result.chunk);
  REQUIRE(vm.run()); CHECK(out.str() == "5\n");
}


TEST_CASE("method closures retain @this even for write-only captures", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    def Write = (number) -> void;
    def Counter {
      number Value = 0;
      Write Make() {
        return func(number value) -> void { @this.Value = value; };
      }
    }
    auto counter = Counter {};
    auto write = counter.Make(); write(42); post(counter.Value);
  )") == "42\n");
}

TEST_CASE("observable recursion is bounded and deferred captures do not inherit mutable refinements", "[callbacks]") {
  CHECK(execute_callback_test(R"(
    observable number value = 0;
    value.OnChange(func(number current) -> void { value = current + 1; });
    try { value = 1; } catch (e) { post("bounded"); }
    post(value > 1);
  )") == "bounded\ntrue\n");
  CHECK_FALSE(clpp::Compiler{}.compile(R"(
    optional<number> value = 1;
    if (value != null) { auto read = func() -> number { return value; }; }
  )").ok());
}
