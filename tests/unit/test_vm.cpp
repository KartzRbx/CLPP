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

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
      "let values = args();\n"
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
  const clpp::CompileResult missing = clpp::Compiler{}.compile("func f() -> int { post(1); }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "missing return");
  const clpp::CompileResult dead = clpp::Compiler{}.compile("func f() -> int { return 1; post(2); }\n");
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

TEST_CASE("compile let and post the local", "[pipeline]") {
  const std::string source = "let x = 1 + 2;\npost(x);\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  REQUIRE(result.chunk.local_count == 1);

  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile assignment updates the local", "[pipeline]") {
  const std::string source = "let mut x = 1;\nx = x + 1;\npost(x);\n";
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

  const clpp::CompileResult redeclared = clpp::Compiler{}.compile("let x = 1; let x = 2;");
  REQUIRE_FALSE(redeclared.ok());
  REQUIRE(redeclared.diagnostics[0].message == "already declared");
}

TEST_CASE("compile while sums one through ten", "[pipeline]") {
  const std::string source =
      "let mut total = 0;\n"
      "let mut i = 1;\n"
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

TEST_CASE("compile local after a block-scoped let", "[pipeline]") {
  const std::string source = "if (0) { let hidden = 1; }\nlet shown = 7;\npost(shown);\n";
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
  const std::string source = "func three() {\n  return 1 + 2;\n}\npost(three());\n";
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
  const std::string source = "func add(a, b) {\n  return a + b;\n}\npost(add(1, 2));\n";
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

  const clpp::CompileResult arity = clpp::Compiler{}.compile("func one(a) { return a; }\npost(one());");
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
  const clpp::CompileResult result = clpp::Compiler{}.compile("int x = \"hi\";");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile accepts an annotated int", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("int x = 1 + 2;\npost(x);\n");
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
      "struct Point {\n"
      "  int x;\n"
      "  int y;\n"
      "}\n"
      "enum Color {\n"
      "  Red,\n"
      "  Green,\n"
      "  Blue\n"
      "}\n"
      "Point p = Point(3, 4);\n"
      "post(p.x + p.y);\n"
      "post(Color.Green);\n";
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
      clpp::Compiler{}.compile("struct Point { int x; int y; }\nPoint p = Point(\"hi\", 1);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile rejects an unknown enum variant", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("enum Color { Red }\npost(Color.Blue);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "unknown variant");
}

TEST_CASE("compile template interpolation", "[pipeline]") {
  const std::string source = "let name = \"CL++\";\npost(`Hello ${name}, ${1 + 2}`);\n";
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

TEST_CASE("compile rejects a template assigned to int", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("int x = `v${1}`;\n");
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

TEST_CASE("compile self table field through @ and @this", "[pipeline]") {
  const std::string source = "@coins = 10;\n@this.coins = @coins + 5;\npost(@coins);\n";
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
  const clpp::CompileResult result = clpp::Compiler{}.compile("@flag = 7;\npost(@this::flag);\n");
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
      return std::string("func add(a, b) { return a + b; }\nfunc twice(n) { return add(n, n); }\n");
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
      "let score = 2;\n"
      "match (score) {\n"
      "  1 ~> post(10);\n"
      "  2 guard score > 10 ~> post(1);\n"
      "  2 guard score > 0 ~> post(20);\n"
      "  _ ~> post(0);\n"
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
  const clpp::CompileResult result = clpp::Compiler{}.compile("match (\"a\") { 1 ~> post(1); }\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile await reads an async function", "[pipeline]") {
  const std::string source = "async func add(a, b) { return a + b; }\npost(await add(1, 2));\n";
  const clpp::CompileResult result = clpp::Compiler{}.compile(source);
  REQUIRE(result.ok());
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(result.chunk);
  REQUIRE(vm.run());
  REQUIRE(out.str() == "3\n");
}

TEST_CASE("compile rejects an async call used as int", "[pipeline]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("async func add(a, b) { return a + b; }\nint x = add(1, 2);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile spawn stores a task that await reads", "[pipeline]") {
  const std::string source =
      "async func add(a, b) { return a + b; }\n"
      "task job = spawn add(1, 2);\n"
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

TEST_CASE("compile rejects spawn stored as int", "[pipeline]") {
  const clpp::CompileResult result =
      clpp::Compiler{}.compile("async func add(a, b) { return a + b; }\nint x = spawn add(1, 2);\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "type mismatch");
}

TEST_CASE("compile parallel lists async results", "[pipeline]") {
  const std::string source =
      "async func add(a, b) { return a + b; }\n"
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

TEST_CASE("ternary range slice bits defaults try and lambda", "[pipeline]") {
  const std::string source =
      "func inc(int x = 1) -> int { return x + 1; }\n"
      "func first(... values) { return values[0]; }\n"
      "func id<T>(T value) where T: int { return value; }\n"
      "namespace Demo { func ping() { return 4; } }\n"
      "post(0 ? 2 : 3);\n"
      "post(1 && 0);\n"
      "post(0 || 4);\n"
      "post(6 & 3);\n"
      "post(1 | 2);\n"
      "post(1 shl 3);\n"
      "post(~0);\n"
      "let mut n = 1;\n"
      "n++;\n"
      "post(n);\n"
      "post(\"abcd\"[1 .. 3]);\n"
      "post(inc());\n"
      "post(inc(x: 4));\n"
      "post(first(7, 8));\n"
      "post((x => x + 1)(2));\n"
      "array<int> xs = array<int>(1, 2, 3);\n"
      "post(xs[1]);\n"
      "dictionary<string, int> sheet = dictionary<string, int>(\"a\", 9);\n"
      "post(sheet[\"a\"]);\n"
      "if (let y = 5) { post(y); }\n"
      "let (a, b) = list(8, 9);\n"
      "post(a);\n"
      "post(b);\n"
      "post(Demo.ping());\n"
      "post(id<int>(3));\n"
      "try { post(1 / 0); } catch (e) { post(0); }\n"
      "[[server]]\n"
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
      "abstract struct Animal {\n"
      "  private int secret;\n"
      "  func reveal() { return self.secret; }\n"
      "  abstract func speak() {}\n"
      "}\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "  override func speak() { return self.bones; }\n"
      "}\n"
      "struct Flyer { int lift; }\n"
      "struct Runner { int pace; }\n"
      "struct Hero : Flyer, Runner { int power; }\n"
      "struct Box<T> { T item; }\n"
      "enum Msg { Text(string), Num(int) }\n"
      "func id<T>(T value) { return value; }\n"
      "Dog pet = Dog(9, 4);\n"
      "post(pet.reveal());\n"
      "post(pet.speak());\n"
      "Hero hero = Hero(3, 4, 5);\n"
      "post(hero.lift);\n"
      "post(hero.power);\n"
      "Box<int> box = Box<int>(8);\n"
      "post(box.item);\n"
      "post(id<int>(3));\n"
      "Msg note = Msg.Text(\"hi\");\n"
      "match (note) { Text(s) ~> post(s); Num(n) ~> post(n); }\n"
      "Option<int> item = Some(6);\n"
      "match (item) { Some(v) ~> post(v); None ~> post(0); }\n"
      "Result<int, string> bad = Err(\"no\");\n"
      "match (bad) { Ok(v) ~> post(v); Err(e) ~> post(e); }\n"
      "let x = 1;\n"
      "if (1) { let x = 2; post(x); }\n"
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
      clpp::Compiler{}.compile("struct Animal { private int secret; }\nAnimal pet = Animal(1);\npost(pet.secret);\n");
  REQUIRE_FALSE(hidden.ok());
  REQUIRE(hidden.diagnostics[0].message == "private member");

  const clpp::CompileResult bare = clpp::Compiler{}.compile("abstract struct Animal { abstract func speak() {} }\nAnimal pet = Animal();\n");
  REQUIRE_FALSE(bare.ok());
  REQUIRE(bare.diagnostics[0].message == "abstract type");

  const clpp::CompileResult missing = clpp::Compiler{}.compile("struct Dog { override func speak() { return 1; } }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "nothing to override");
}

TEST_CASE("async call waits until await and spawn runs it", "[pipeline]") {
  const clpp::CompileResult deferred = clpp::Compiler{}.compile(
      "async func later() { post(1); return 2; }\n"
      "let job = later();\n"
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
      "async func later() { post(7); return 8; }\n"
      "task job = spawn later();\n"
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
      "let mut x = 4;\n"
      "post(-x);\n"
      "x += 1;\n"
      "post(x);\n"
      "const name = 7;\n"
      "post(name);\n"
      "if (true) { post(1); } else { post(0); }\n"
      "if (false) { post(8); } else { post(2); }\n"
      "if (null) { post(8); } else { post(3); }\n"
      "let mut total = 0;\n"
      "for (let mut k = 0; k < 4; k += 1) { total = total + k; }\n"
      "post(total);\n"
      "let mut seen = 0;\n"
      "let mut k = 0;\n"
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
  const clpp::CompileResult result = clpp::Compiler{}.compile("const x = 1;\nx = 2;\n");
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

TEST_CASE("compile rejects assignment to let", "[pipeline]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("let x = 1;\nx = 2;\n");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
  REQUIRE(result.diagnostics[0].message == "cannot assign to immutable binding");
}

TEST_CASE("compile exhaustive enum match", "[pipeline]") {
  const std::string source =
      "enum Color { Red, Green, Blue }\n"
      "let color = Color.Green;\n"
      "match (color) {\n"
      "  Red ~> post(0);\n"
      "  Green ~> post(1);\n"
      "  Blue ~> post(2);\n"
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
      "enum Color { Red, Green, Blue }\n"
      "let color = Color.Green;\n"
      "match (color) { Red ~> post(0); }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.chunk.code.empty());
  REQUIRE(missing.diagnostics[0].message == "non-exhaustive match");

  const clpp::CompileResult guarded = clpp::Compiler{}.compile(
      "enum Color { Red, Green }\n"
      "let color = Color.Red;\n"
      "match (color) {\n"
      "  Red guard false ~> post(0);\n"
      "  Green ~> post(1);\n"
      "}\n");
  REQUIRE_FALSE(guarded.ok());
  REQUIRE(guarded.diagnostics[0].message == "non-exhaustive match");

  const clpp::CompileResult wildcard = clpp::Compiler{}.compile(
      "enum Color { Red, Green, Blue }\n"
      "let color = Color.Blue;\n"
      "match (color) { _ ~> post(2); }\n");
  REQUIRE(wildcard.ok());
}

TEST_CASE("compile the remaining statement syntax", "[pipeline]") {
  const std::string source =
      "import @clpp.math as Math;\n"
      "using Math.abs;\n"
      "struct Point { int x; int y; }\n"
      "signal ping;\n"
      "static func show(int x) { post(x); }\n"
      "func one() { return 1; }\n"
      "auto base = 3;\n"
      "observable n = 1;\n"
      "n = n + 1;\n"
      "constexpr doubled = 2 + 2;\n"
      "cout(base);\n"
      "warn(n);\n"
      "try { report(doubled); } catch (e) { post(e); }\n"
      "post(abs(0 - 5));\n"
      "post(endl);\n"
      "Vector2 v = Vector2(1, 2);\n"
      "post(v.y);\n"
      "post(Vector4(1, 2, 3, 4).w);\n"
      "post(Vector2(8, 9)[1]);\n"
      "post(\"AZ\"[1]);\n"
      "let mut sum = 0;\n"
      "for (let i in 4) { sum = sum + i; }\n"
      "post(sum);\n"
      "post(pcall(1 / 0));\n"
      "post(pcall(8 / 2));\n"
      "ping ~> show;\n"
      "ping(6);\n"
      "post(one());\n"
      "post(new Point(3, 4).x);\n";
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

  const clpp::CompileResult folded = clpp::Compiler{}.compile("constexpr x = name;\n");
  REQUIRE_FALSE(folded.ok());
  REQUIRE(folded.diagnostics[0].message == "not a constant");
}

TEST_CASE("register add stores into the destination register", "[register]") {
  const std::string source = "let a = 5;\nlet b = 30;\nlet c = a + b;\npost(c);\n";
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
      "let mut text = \"hi\";\n"
      "let taken = move(text);\n"
      "post(taken);\n"
      "post(text);\n"
      "struct Point { int x; int y; }\n"
      "func operator+(Point a, Point b) { return a.x + b.x; }\n"
      "Point p = Point(1, 2);\n"
      "Point q = Point(3, 4);\n"
      "post(p + q);\n"
      "variant Box { int, string }\n"
      "let box = Box(7);\n"
      "post(box);\n"
      "let word = Box(\"ok\");\n"
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

  const clpp::CompileResult frozen = clpp::Compiler{}.compile("let text = \"hi\";\nlet taken = move(text);\n");
  REQUIRE_FALSE(frozen.ok());
  REQUIRE(frozen.diagnostics[0].message == "cannot move");
}

TEST_CASE("native threads join and atomic fetch_add", "[pipeline]") {
  const std::string source =
      "func work(int x) { return x; }\n"
      "let a = thread work(20);\n"
      "let b = thread work(22);\n"
      "post(join(a) + join(b));\n"
      "atomic n = 0;\n"
      "let gate = mutex();\n"
      "func inc(id, mid) {\n"
      "  lock(mid);\n"
      "  fetch_add(id, 1);\n"
      "  unlock(mid);\n"
      "  return 1;\n"
      "}\n"
      "let c = thread inc(n, gate);\n"
      "let d = thread inc(n, gate);\n"
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
      "extern func strlen(string s);\n"
      "let v = list(3, 1, 2);\n"
      "let s = sort(v);\n"
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

TEST_CASE("gradual modes unions and int add", "[pipeline]") {
  const std::string strict_source =
      "<<!strict\n"
      "type State = \"Idle\" | \"Running\";\n"
      "type Same = int & int;\n"
      "let state: State = \"Idle\";\n"
      "let n: Same = 4;\n"
      "let x: int = 2;\n"
      "let y: int = 3;\n"
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
      "<<!strict\ntype State = \"Idle\" | \"Running\";\nlet state: State = \"Dead\";\n");
  REQUIRE_FALSE(outside.ok());
  REQUIRE(outside.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult sleeping = clpp::Compiler{}.compile(
      "<<!strict\ntype State = \"Idle\" | \"Running\";\nlet state: State = \"Idle\";\nif (state == \"Sleeping\") { post(0); }\n");
  REQUIRE_FALSE(sleeping.ok());
  REQUIRE(sleeping.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult crossed = clpp::Compiler{}.compile("type Bad = int & string;\nlet y: Bad = 1;\n");
  REQUIRE_FALSE(crossed.ok());
  REQUIRE(crossed.diagnostics[0].message == "type mismatch");

  const clpp::CompileResult nocheck = clpp::Compiler{}.compile("<<!nocheck\nint x = \"hi\";\npost(1);\n");
  REQUIRE(nocheck.ok());
  std::ostringstream unchecked;
  clpp::VirtualMachine loose;
  loose.set_output(unchecked);
  loose.load(nocheck.chunk);
  REQUIRE(loose.run());
  REQUIRE(unchecked.str() == "1\n");
}

TEST_CASE("struct inheritance dispatches the dynamic method", "[pipeline]") {
  const std::string source =
      "struct Animal {\n"
      "  int age;\n"
      "  func speak() { return self.age; }\n"
      "}\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "  func speak() { return self.bones; }\n"
      "}\n"
      "struct Puppy : Dog {\n"
      "  int toys;\n"
      "  func speak() { return self.toys; }\n"
      "}\n"
      "struct Counter {\n"
      "  func add(int x) { return x; }\n"
      "}\n"
      "struct Plus : Counter {\n"
      "  func add(int x) { return x + 1; }\n"
      "}\n"
      "Dog pet = Dog(4, 9);\n"
      "post(pet.age);\n"
      "post(pet.bones);\n"
      "post(pet.speak());\n"
      "Animal view = pet;\n"
      "post(view.speak());\n"
      "post(view.age);\n"
      "Puppy pup = Puppy(1, 2, 8);\n"
      "Animal any = pup;\n"
      "post(any.speak());\n"
      "Plus extra = Plus();\n"
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

  const clpp::CompileResult missing = clpp::Compiler{}.compile("struct Dog : Missing { int bones; }\n");
  REQUIRE_FALSE(missing.ok());
  REQUIRE(missing.diagnostics[0].message == "unknown type");

  const clpp::CompileResult hidden =
      clpp::Compiler{}.compile("struct Animal { int age; }\nstruct Dog : Animal { int age; }\n");
  REQUIRE_FALSE(hidden.ok());
  REQUIRE(hidden.diagnostics[0].message == "already declared");

  const clpp::CompileResult bones = clpp::Compiler{}.compile(
      "struct Animal { int age; }\nstruct Dog : Animal { int bones; }\nAnimal view = Dog(1, 2);\npost(view.bones);\n");
  REQUIRE_FALSE(bones.ok());
  REQUIRE(bones.diagnostics[0].message == "unknown field");

  const clpp::CompileResult shadow = clpp::Compiler{}.compile(
      "let x = 1;\nif (1) { let x = 2; post(x); }\npost(x);\n");
  REQUIRE(shadow.ok());
  std::ostringstream shadow_out;
  clpp::VirtualMachine shadow_vm;
  shadow_vm.set_output(shadow_out);
  shadow_vm.load(shadow.chunk);
  REQUIRE(shadow_vm.run());
  REQUIRE(shadow_out.str() == "2\n1\n");

  const clpp::CompileResult diamond = clpp::Compiler{}.compile(
      "struct A { int health; }\nstruct B : A { int armor; }\nstruct C : A { int speed; }\n"
      "struct D : B, C { int extra; }\nD value = D(1, 2, 3, 4);\n"
      "post(value.health);\npost(value.armor);\npost(value.speed);\npost(value.extra);\n");
  REQUIRE(diamond.ok());
  std::ostringstream diamond_out;
  clpp::VirtualMachine diamond_vm;
  diamond_vm.set_output(diamond_out);
  diamond_vm.load(diamond.chunk);
  REQUIRE(diamond_vm.run());
  REQUIRE(diamond_out.str() == "1\n2\n3\n4\n");

  const clpp::CompileResult clash = clpp::Compiler{}.compile(
      "struct Left { int x; }\nstruct Right { int x; }\nstruct Both : Left, Right { int y; }\n");
  REQUIRE_FALSE(clash.ok());
  REQUIRE(clash.diagnostics[0].message == "ambiguous inheritance");

  const clpp::CompileResult remembered = clpp::Compiler{}.compile(
      "struct Animal {\n"
      "  int age;\n"
      "  func speak() { return self.age; }\n"
      "}\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "  func speak() { return self.bones; }\n"
      "}\n"
      "let pet = Dog(4, 9);\n"
      "let copy = pet;\n"
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
      "func step() {\n"
      "  coroutine.yield(1);\n"
      "  return 2;\n"
      "}\n"
      "func later() { post(7); }\n"
      "func twice(int n) { return n + n; }\n"
      "let co = coroutine.create(step);\n"
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
      "func probe(int n) { return fs::size(\"missing\"); }\npost(actor(probe, 1));\n");
  REQUIRE(blocked.ok());
  clpp::VirtualMachine sandbox;
  sandbox.load(blocked.chunk);
  REQUIRE_FALSE(sandbox.run());
  REQUIRE(sandbox.error() == "sandbox");
}

TEST_CASE("missing initializer stack overflow and crlf", "[pipeline]") {
  const clpp::CompileResult incomplete = clpp::Compiler{}.compile("let a =\nint x = \"hi\";\n");
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

  const clpp::CompileResult recursive = clpp::Compiler{}.compile("func dive() { dive(); }\ndive();\n");
  REQUIRE(recursive.ok());
  clpp::VirtualMachine deep;
  deep.load(recursive.chunk);
  REQUIRE_FALSE(deep.run());
  REQUIRE(deep.error() == "stack overflow");
}

TEST_CASE("observable assignment runs OnChange", "[pipeline]") {
  const std::string source =
      "observable int coins = 100;\n"
      "coins.OnChange(func (int newValue) { post(\"now \" .: newValue); });\n"
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
post(Axiom.Round(1.234, 2));
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
post(Axiom.Round(Axiom.InSine(1), 6));
post(Axiom.Round(Axiom.OutBounce(1), 4));
post(Axiom.CubicBezier(0, Vector3(1, 0, 0), Vector3(2, 0, 0), Vector3(3, 0, 0), Vector3(4, 0, 0)).x);
post(Axiom.Hover(0, Vector3(1, 2, 3), Vector3(0, 0, -1)).y);
post(Axiom.AabbContains(Vector3(0, 0, 0), Vector3(-1, -1, -1), Vector3(1, 1, 1)));
post(Axiom.SphereContains(Vector3(0, 0, 0), Vector3(0, 0, 0), 1));
post(Axiom.RayPlane(Vector3(0, 0, 0), Vector3(0, -1, 0), Vector3(0, -1, 0), Vector3(0, 1, 0)).y);
post(Axiom.Barycentric(Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0)).x);
post(Axiom.ClosestPointOnSegment(Vector3(0.5, 0, 0), Vector3(0, 0, 0), Vector3(1, 0, 0)).x);
post(Axiom.Deg(Axiom.Rad(90)));
post(Axiom.Round(Axiom.Atan2(1, 0), 4));
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
  const char* sources[] = {"post(1);\n", "let x = 1;\npost(x + 2);\n", "func add(a, b) { return a + b; }\npost(add(1, 2));\n"};
  for (const char* source : sources) {
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE(result.ok());
    const clpp::BytecodeReport report = clpp::verify_bytecode_report(result.chunk);
    REQUIRE(report.ok());
    REQUIRE_FALSE(result.chunk.source_map.empty());
    REQUIRE(result.chunk.source_map.front().location.line >= 1);
    REQUIRE(clpp::check_backend(clpp::Backend::Vm, result.chunk).empty());
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

TEST_CASE("luau text backend prints a local and rejects host opcodes", "[luau]") {
  const clpp::AnalysisResult analysis = clpp::analyze_program("let x = 1;\npost(x + 2);\n", {});
  const clpp::LuauEmit emitted = clpp::clir_to_luau(analysis);
  REQUIRE(emitted.error.empty());
  REQUIRE(emitted.source == "local x = 1\nprint(x + 2)\n");

  const clpp::CompileResult host =
      clpp::Compiler{}.compile("link @clpp.http as Http;\npost(Http.host(\"http://h.test/a\"));\n");
  REQUIRE(host.ok());
  REQUIRE(clpp::check_backend(clpp::Backend::Luau, host.chunk) == "luau backend: unsupported host opcode");
  REQUIRE(clpp::check_backend(clpp::Backend::Vm, host.chunk).empty());
}
