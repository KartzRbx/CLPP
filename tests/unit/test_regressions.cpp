// Regression tests for the problems found in the CL++ rework review.
#include "clpp/clir.hpp"
#include "clpp/compiler.hpp"
#include "clpp/core/lexer/lexer.hpp"
#include "clpp/vm.hpp"
#include "clpp/stdlib.hpp"
#include "core/compiler/analyze.hpp"
#include "stdlib/decimal_parse.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace {

struct Run {
  bool compiled{false};
  bool ran{false};
  std::string output;
  std::string error;
  std::string detail;
};

Run run(const std::string& source, const std::map<std::string, std::string>& files = {}) {
  Run result;
  const clpp::ModuleLoader loader = [&files](const std::string_view path) -> std::optional<std::string> {
    const auto found = files.find(std::string(path));
    if (found == files.end()) {
      return std::nullopt;
    }
    return found->second;
  };
  const clpp::CompileResult compiled = clpp::Compiler{}.compile(source, loader);
  result.compiled = compiled.ok();
  if (!compiled.ok()) {
    result.error = compiled.diagnostics.front().message;
    result.detail = "line " + std::to_string(compiled.diagnostics.front().location.line) + " near " + std::string(compiled.diagnostics.front().span);
    return result;
  }
  std::ostringstream out;
  clpp::VirtualMachine vm;
  vm.set_output(out);
  vm.load(compiled.chunk);
  result.ran = vm.run();
  result.output = out.str();
  result.error = std::string(vm.error());
  return result;
}

}  // namespace

TEST_CASE("only @this denotes the implicit method receiver", "[instance-expression]") {
  for (const std::string source : {
    "def Item { number Value; number Read() { return self.Value; } }",
    "def Item { number Value; void Write() { self.Value = 7; } }",
    "def Item { Item Same() { return self; } }",
    "def Item<T> { T Value; T Read() { return self.Value; } }",
    "def Item { number Value; async number Read() { return self.Value; } }",
    "def Item { number Value; number Read() { auto get=func() -> number { return self.Value; }; return get(); } }",
    "def Item { number Value; } number Item::Read() { return self.Value; }"
  }) {
    INFO(source);
    const auto result = run(source);
    CHECK_FALSE(result.compiled);
  }
  const auto result = run(R"(
    def Getter<T> = () -> T;
    def Box<T> {
      T Value;
      Getter<T> Reader() { return func() -> T { return @this.Value; }; }
      Box<T> Same() { return @this; }
      void Set(T value) { number self = 99; @this.Value = value; }
      async T Later() { return @this.Value; }
    }
    T Box::Read() { return @this.Value; }
    auto box = Box<number> { Value = 7 };
    auto read = box.Reader(); box.Set(42);
    post(read()); post(box.Read()); post(await box.Later()); post(box.Same() == box);
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n42\n42\ntrue\n");
}

TEST_CASE("legacy data and named function declarations are rejected", "[legacy-declarations]") {
  for (const char* source : {
    "struct Item { number Value; }", "final struct Item { number Value; }",
    "abstract struct Item { abstract number Read(); }",
    "func Read() { return 1; }", "func Read() -> number { return 1; }",
    "async func Read() -> number { return 1; }",
    "def Item { func Read() -> number { return 1; } }",
    "def Item { template <typename T> func Read(T value) -> T { return value; } }",
    "namespace Legacy { func Read() { return 1; } }",
    "extern func strlen(string text);", "auto callback = func(number value) { return value; };",
    "post((value => value + 1)(2));"
  }) {
    INFO(source); CHECK_FALSE(run(source).compiled);
  }
  const auto result = run(R"(
    def Transform = (number) -> number;
    number Read(number value) { return value; }
    async number Later(number value) { return value + 1; }
    def Item { number Value; number Read() { return @this.Value; } }
    Transform callback = func(value) -> number { return value * 2; };
    extern number strlen(string text);
    post(Read(20)); post(await Later(20)); post(callback(21));
    post(Item { Value = 42 }.Read()); post(strlen("CL++"));
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "20\n21\n42\n42\n4\n");
}

TEST_CASE("obsolete syntax has no compatibility path in either checking mode", "[language-cleanup]") {
  for (const char* mode : {"pragma strict; ", "pragma nostrict; "}) {
    for (const char* source : {
        "variant Value { number, string }",
        "type Value = number | string;",
        "namespace Legacy { number Read() { return 1; } }",
        "enum Legacy { First, Second }",
        "enum class Legacy { Payload(number) }",
        "enum class Legacy { First } post(Legacy.First);",
        "import @clpp.math as Math;", "from @clpp.math as Math;",
        "link @clpp.math as Math; using Math.abs;",
        "static number Read() { return 1; }", "static auto value = 1;",
        "[[server]] post(1);", "cout(1);", "post(endl);",
        "def Item { number Value; } auto item = new Item { Value = 1 };",
        "def Item { number Value; } auto item = Item(1);",
        "def Item { number Value = 1; } auto item = Item();",
        "def Item { number Value; number Read() { return @Value; } }",
        "def Item { number Value; number Read() { return @this::Value; } }",
        "@Value = 1; post(@Value);",
        "signal event;", "signal<number> event; void Handler(number value) {} event~>Handler;",
        "signal<number> event; event(1);",
        "match (1) { 1 ~> post(1); _ ~> post(0); }",
        "optional<number> value = Some(1);", "optional<number> value = None;",
        "optional<number> value = None();", "auto value = Ok(1);", "auto value = Err(1);",
        "Option<number> value = null;", "Result<number, string> value = 1;",
        "number Read(value) { return value; }",
        "async number Read() { return 1; } task value = spawn Read();",
        "observable value = 1;"
    }) {
      INFO(mode); INFO(source);
      const auto result = run(std::string(mode) + source);
      CHECK_FALSE(result.compiled);
    }
  }
  const auto result = run(R"(
    def Value = number | string;
    def Result<T> { T Value; }
    number Some(number value) { return value; }
    number Ok(number value) { return value; }
    enum class Kind { Ready, Done }
    def Item { number Value = 3; number Read() { return @this.Value; } }
    optional<number> absent = null;
    Value value = "text";
    Result<number> result = Result<number> { Value = 7 };
    auto item = Item {};
    signal<number> event;
    event~>Connect(func(number payload) -> void { post(payload); });
    event::Fire(item.Read()); event::Clear(); event::Fire(99);
    post(result.Value); post(value); post(absent); post(Some(4)); post(Ok(5));
    match (Kind::Ready) { Kind::Ready => post(6); Kind::Done => post(0); }
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "3\n7\ntext\nnull\n4\n5\n6\n");
}

TEST_CASE("a line comment in a CRLF file does not swallow the program", "[regression][lexer]") {
  const Run result = run("<< comment\r\npost(\"hello\");\r\npost(1);\r\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "hello\n1\n");
}

TEST_CASE("do while executes once and continue reaches the post condition", "[do-while]") {
  const auto result = run(R"(
    number once=0; do { once+=1; } while(false); post(once);
    number turns=0; number checks=0;
    bool More() { checks+=1; return turns<3; }
    do { turns+=1; if(turns<3) { continue; } post(turns); } while(More());
    post(checks);
    do { break; } while(More()); post(checks);
    number total=0;
    for(number i=0;i<2;i++) {
      number inner=0;
      do { inner+=1; switch(inner) { case 1: continue; default: break; } total+=inner; } while(inner<2);
    }
    post(total);
    do { try { report("retry"); } catch(message) { post(message); break; } } while(true);
    number ReturnValue() { do { return 42; } while(false); return 0; }
    post(ReturnValue());
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output=="1\n3\n3\n3\n4\nretry\n42\n");
}

TEST_CASE("do while preserves body scope and never narrows its first iteration", "[do-while]") {
  for(const char* source : {
    "do { number local=1; } while(local>0);",
    "do { post(1); } while(\"invalid\");",
    "do { post(1); } while(false)",
    "do post(1); while(false);",
    "def Item { number Value; } optional<Item> item=null; do { post(item.Value); } while(item!=null);",
    "do { number local=1; } while(false); post(local);"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
  const auto result=run(R"(
    def Item { number Value; } optional<Item> item=Item { Value=7 };
    do { guard(item!=null) else { break; } post(item.Value); item=null; } while(item!=null);
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output=="7\n");
}

TEST_CASE("do while survives CLIR and cooperative task suspension", "[do-while][bytecode]") {
  const auto compiled=clpp::Compiler{}.compile(R"(
    async number Step(number value) { return value+1; }
    async number Count() {
      number value=0;
      do { value=await Step(value); } while(value<3);
      return value;
    }
    post(await Count());
  )");
  REQUIRE(compiled.ok());
  const auto restored=clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk));
  std::ostringstream output; clpp::VirtualMachine vm; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str()=="3\n");
}

TEST_CASE("a lone CR also ends a line comment", "[regression][lexer]") {
  const Run result = run("<< old mac\rpost(2);\r");
  REQUIRE(result.output == "2\n");
}

TEST_CASE("number division truncates toward zero, float division does not", "[regression][semantics]") {
  const Run result = run("post(7 / 2);\npost(-7 / 2);\npost(7.0 / 2);\nconst auto a = 9;\nconst auto b = 4;\npost(a / b);\npost(a % b);\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n-3\n3.5\n2\n1\n");
}

TEST_CASE("whole numbers print exactly", "[regression][vm]") {
  const Run result = run("auto s = 0;\nauto i = 0;\nwhile (i < 100000) { s += i; i += 1; }\npost(s * 1000);\npost(0.5);\n");
  REQUIRE(result.output == "4999950000000\n0.5\n");
}

TEST_CASE("deep recursion does not overflow at 256 calls", "[regression][vm]") {
  const Run result = run("number d(number n) {\n  if (n == 0) { return 0; }\n  return 1 + d(n - 1);\n}\npost(d(50000));\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "50000\n");
}

TEST_CASE("modules export structs with inheritance, enums, functions and constants", "[regression][modules]") {
  const std::map<std::string, std::string> files = {
      {"./shapes.clp",
       "export const auto MAX_HP = 100;\nexport def Animal {\n  number legs;\n  any speak() { return @this.legs * 10; }\n}\nexport def Dog : Animal {\n  number bones;\n}\nexport enum class Color { Red, Green, Blue }\nexport number area(number w, number h) { return w * h; }\nexport number twiceArea(number w, number h) {\n  auto total = 0;\n  for (auto i = 0; i < 2; i += 1) { total += area(w, h); }\n  return total;\n}\nexport number clampHp(number hp) {\n  if (hp > MAX_HP) { return MAX_HP; }\n  return hp;\n}\n"}};
  const Run result = run(
      "link \"./shapes.clp\" as Shapes;\n"
      "Shapes.Dog d = Shapes.Dog { legs = 4, bones = 2 };\n"
      "post(d.legs);\n"
      "post(d.speak());\n"
      "post(Shapes.twiceArea(3, 4));\n"
      "post(Shapes.Color::Green);\n"
      "post(Shapes.MAX_HP);\n"
      "post(Shapes.clampHp(250));\n",
      files);
  REQUIRE(result.compiled);
  REQUIRE(result.output == "4\n40\n24\n1\n100\n100\n");
}

TEST_CASE("an error inside a module is reported on the link line", "[regression][modules]") {
  const Run result = run("link \"./bad.clp\" as Bad;\npost(1);\n", {{"./bad.clp", "export any f() {\n  return nope;\n}\n"}});
  REQUIRE_FALSE(result.compiled);
  REQUIRE(result.error.find("error in module ./bad.clp (2:10)") == 0);
}

TEST_CASE("a module initializes top-level code", "[regression][modules]") {
  const Run result = run("link \"./m.clp\" as M;\n", {{"./m.clp", "post(1);\n"}});
  REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "1\n");
}

TEST_CASE("top-level constants are visible in functions unless shadowed", "[regression][semantics]") {
  const Run result = run("const auto X = 1;\nnumber f(number X) { return X; }\nnumber g() { const auto X = 5; return X; }\nnumber h() { return X + 1; }\n"
                         "post(f(7));\npost(g());\npost(h());\n");
  REQUIRE(result.output == "7\n5\n2\n");
}

TEST_CASE("diagnostics own their message after the analysis is gone", "[regression][api]") {
  clpp::CompileResult result = clpp::Compiler{}.compile("link \"./x.clp\" as X;\n", [](std::string_view) {
    return std::optional<std::string>("number f() { return nope; }\n");
  });
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.diagnostics.front().message.find("error in module") == 0);
}

TEST_CASE("VM preserves grouping, string escapes and numeric values", "[regression][vm]") {
  const Run result = run(
      "post((1 + 2) * 3);\n"
      "post(10 - (3 - 2));\n"
      "post(\"say \\\"hi\\\"\");\n"
      "post(3.14159265);\n"
      "post(7 / 2);\n");
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "9\n9\nsay \"hi\"\n3.14159265\n3\n");
}

TEST_CASE("unary operators precede arithmetic and comparisons", "[regression][precedence]") {
  const Run result = run("post(-2 * 3 + 1); post(4 * -2); post(-2 < 1); post(not true == false); post(1 + 2 * 3 == 7 and true);");
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "-5\n-8\ntrue\ntrue\ntrue\n");
}

TEST_CASE("len counts elements, dictionary entries and UTF-8 characters", "[regression]") {
  const Run result = run(
      "post(len(list(4, 5, 6)));\n"
      "post(len(\"ação\"));\n"
      "dictionary<string, number> d = dictionary<string, number>(\"a\", 1, \"b\", 2);\n"
      "post(len(d));\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n4\n2\n");
  REQUIRE_FALSE(run("post(len(3));\n").compiled);
}

TEST_CASE("for-in walks lists, strings and dictionary keys; numbers still count", "[regression]") {
  const Run result = run(
      "for (auto v in list(5, 6)) { post(v); }\n"
      "for (auto c in \"hé\") { post(c); }\n"
      "dictionary<string, number> d = dictionary<string, number>(\"x\", 1);\n"
      "for (auto k in d) { post(k); }\n"
      "for (auto i in 2) { post(i); }\n"
      "for (auto v in list(1, 2, 3)) { if (v == 2) { break; } post(v); }\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "5\n6\nh\né\nx\n0\n1\n1\n");
}

TEST_CASE("fields, nested fields, vector components and list elements can be assigned", "[regression]") {
  const Run result = run(
      "def Stats { number hp; number mana; }\n"
      "def Player { string name; Stats stats; Vector3 pos; }\n"
      "Player p = Player { name = \"Ada\", stats = Stats { hp = 100, mana = 50 }, pos = Vector3(0, 0, 0) };\n"
      "p.name = \"Bea\";\n"
      "p.stats.hp -= 30;\n"
      "p.pos.y = 4.5;\n"
      "post(p.name .: \" \" .: p.stats.hp .: \" \" .: p.pos.y);\n"
      "auto xs = list(1, 2, 3);\n"
      "xs[1] = 20;\n"
      "xs[2] *= 3;\n"
      "post(xs[1] .: \" \" .: xs[2]);\n"
      "array<Stats> team = array<Stats>(Stats { hp = 1, mana = 1 }, Stats { hp = 2, mana = 2 });\n"
      "team[1].hp = 99;\n"
      "post(team[1].hp);\n"
      "dictionary<string, number> bag = dictionary<string, number>(\"gold\", 1);\n"
      "bag[\"gold\"] += 9;\n"
      "bag[\"gem\"] = 2;\n"
      "post(bag[\"gold\"] .: \" \" .: bag[\"gem\"] .: \" \" .: len(bag));\n"
      "Player copy = p;\n"
      "copy.stats.hp = 1;\n"
      "post(p.stats.hp .: \" \" .: copy.stats.hp);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "Bea 70 4.5\n20 9\n99\n10 2 2\n1 1\n");
  REQUIRE_FALSE(run("def S { number hp; }\n"
  "const auto s = S { hp = 1 };\n"
  "s.hp = 2;\n").compiled);
}

TEST_CASE("methods that change @this update the variable they were called on", "[regression]") {
  const Run result = run(
      "def Animal { number age; void birthday() { @this.age += 1; } number years() { return @this.age; } }\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "  number fetch() { @this.bones += 1; @this.birthday(); return @this.bones; }\n"
      "}\n"
      "Dog d = Dog { age = 3, bones = 0 };\n"
      "d.birthday();\n"
      "post(d.fetch());\n"
      "post(d.years());\n"
      "number hurt(Dog other) { other.age = 0; return other.age; }\n"
      "post(hurt(d));\n"
      "post(d.age);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "1\n5\n0\n0\n");
}

TEST_CASE("continue skips to the next iteration of every loop kind", "[regression]") {
  const Run result = run(
      "for (auto i in 5) { if (i % 2 == 0) { continue; } post(i); }\n"
      "for (auto v in list(1, 2, 3)) { if (v == 2) { continue; } post(v); }\n"
      "auto n = 0;\n"
      "while (n < 4) { n = n + 1; if (n == 2) { continue; } post(n); }\n"
      "for (auto j = 0; j < 3; j = j + 1) { if (j == 1) { continue; } post(j); }\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "1\n3\n1\n3\n1\n3\n4\n0\n2\n");
  REQUIRE_FALSE(run("continue;\n").compiled);
}

TEST_CASE("post shows lists, dictionaries, structs and vectors as written; any value joins as text", "[regression]") {
  const Run result = run(
      "def P { string n; number hp; }\n"
      "post(list(1, 2, list(3)));\n"
      "post(list(\"a\", \"b\"));\n"
      "post(P { n = \"a\", hp = 2 });\n"
      "dictionary<string, number> d = dictionary<string, number>(\"g\", 1);\n"
      "post(d);\n"
      "post(Vector2(1.5, 2));\n"
      "post(Vector3(1, 2, 3) + Vector3(1, 1, 1));\n"
      "post(\"v=\" .: Vector3(1, 2, 3));\n"
      "post(\"alive: \" .: true .: \" / \" .: (1 > 2));\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output ==
          "[1, 2, [3]]\n[\"a\", \"b\"]\n(\"a\", 2)\n{g: 1}\n(1.5, 2)\n(2, 3, 4)\nv=(1, 2, 3)\nalive: true / false\n");
}

TEST_CASE("bit operators on variables use the computed value (register alias reset)", "[regression]") {
  const Run result = run(
      "const auto a = 1;\nconst auto b = 2;\npost(a | b);\npost(a & 3);\npost(a ^ b);\npost(a shl b);\npost(8 shr a);\npost(~a);\n"
      "const auto x = 6;\nconst auto y = x | 1;\npost(y);\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n1\n3\n4\n4\n-2\n7\n");
}

TEST_CASE("operator+ applies to constructor calls and fields; distinct text literals compare", "[regression]") {
  const Run result = run(
      "def Money { number cents; }\n"
      "Money operator+(Money a, Money b) { return Money { cents = a.cents + b.cents }; }\n"
      "Money total = Money { cents = 150 } + Money { cents = 275 };\n"
      "post(total.cents);\n"
      "post(\"abc\" != \"abd\");\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "425\ntrue\n");
}

TEST_CASE("missing return is checked on every path; -> void is a return type", "[regression]") {
  REQUIRE_FALSE(run("number sign(number n) {\n  if (n > 0) { return 1; }\n}\n").compiled);
  const Run ok = run(
      "number sign(number n) {\n  if (n > 0) { return 1; } else { return 0; }\n}\n"
      "number forever() { while (true) { return 2; } }\n"
      "number fail() { report(\"no\"); }\n"
      "void log(string m) { post(m); }\n"
      "post(sign(5));\npost(forever());\nlog(\"ok\");\n");
  INFO(ok.error);
  REQUIRE(ok.ran);
  REQUIRE(ok.output == "1\n2\nok\n");
}

TEST_CASE("@field inside a method is @this.field; construction names fields", "[regression]") {
  const Run result = run(
      "def P {\n  string name;\n  number hp;\n"
      "  void hit() { @this.hp = @this.hp - 10; }\n"
      "  void show() { post(@this.name .: \" \" .: @this.hp); }\n}\n"
      "P p = P { hp=50, name=\"Ada\" };\n"
      "p.hit();\np.show();\nnumber coins = 3;\npost(coins);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "Ada 40\n3\n");
}

TEST_CASE("scalar equality compares values and aggregate equality compares identity", "[regression]") {
  const Run result = run(
      "enum class Direction { North, East }\n"
      "const auto facing = Direction::East;\n"
      "post(facing == Direction::East);\n"
      "const auto alive = true;\n"
      "post(alive == true);\n"
      "def P { number x; }\n"
      "P p = P { x = 1 };\n"
      "P q = P { x = 2 };\n"
      "post(p == q);\n"
      "post(p != q);\n"
      "post(list(1, 2) == list(1, 2));\n"
      "post(Vector3(1, 2, 3) == Vector3(1, 2, 3));\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "true\ntrue\nfalse\ntrue\nfalse\ntrue\n");
}

TEST_CASE("push, pop, insert and remove change the list variable; find works on any value", "[regression]") {
  const Run result = run(
      "auto xs = list();\npush(xs, 10);\npush(xs, 20);\npush(xs, 30);\npost(pop(xs));\n"
      "insert(xs, 0, 5);\npost(remove(xs, 1));\npost(xs);\n"
      "dictionary<string, number> inv = dictionary<string, number>(\"gold\", 3, \"gem\", 1);\npost(remove(inv, \"gold\"));\npost(len(inv));\n"
      "auto names = list(\"ana\", \"bia\");\npost(find(names, \"bia\"));\n"
      "array<number> fill() { array<number> out = array<number>(); for (auto i in 3) { push(out, i * i); } return out; }\npost(fill());\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "30\n10\n[5, 20]\n3\n1\n1\n[0, 1, 4]\n");
  REQUIRE_FALSE(run("const auto fixed = list(1);\npush(fixed, 2);\n").compiled);
}

TEST_CASE("@clpp.text: split, joinAll, replace, startsWith, endsWith, repeat, toNumber, indexOf", "[regression]") {
  const Run result = run(
      "link @clpp.text as Text;\n"
      "const auto parts = Text.split(\"a,b,c\", \",\");\npost(parts);\npost(Text.joinAll(parts, \"|\"));\n"
      "post(Text.replace(\"1-2-3\", \"-\", \"+\"));\npost(Text.startsWith(\"CL++\", \"CL\"));\n"
      "post(Text.endsWith(\"save.dat\", \".json\"));\npost(Text.repeat(\"ab\", 3));\n"
      "post(Text.toNumber(\" 42.5 \") + 1);\npost(Text.indexOf(\"ação!\", \"!\"));\n"
      "try { post(Text.toNumber(\"x1\")); } catch (e) { post(e); }\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output ==
          "[\"a\", \"b\", \"c\"]\na|b|c\n1+2+3\ntrue\nfalse\nababab\n43.5\n4\nnot a number: \"x1\"\n");
}

TEST_CASE("native text conversion preserves large values and rejects embedded NUL", "[regression][native-text]") {
  using clpp::Value;
  std::string error;
  Value out;
  Value args[] = {Value::struct_of({Value::integer_of(9223372036854775807LL),
                                   Value::integer_of(-9223372036854775807LL - 1),
                                   Value::number_of(1e300), Value::number_of(-1e300)}),
                  Value::string_of(",")};
  REQUIRE(clpp::stdlib::std_apply(14, args, 2, out, error));
  REQUIRE(out.text == "9223372036854775807,-9223372036854775808,1.0000000000000001e+300,-1.0000000000000001e+300");
  args[0] = Value::string_of(std::string("12\0junk", 7));
  REQUIRE_FALSE(clpp::stdlib::std_apply(12, args, 1, out, error));
}

TEST_CASE("native text repeat rejects nonfinite counts before integer conversion", "[regression][native-text]") {
  using clpp::Value;
  Value args[] = {Value::string_of("a"), Value::number_of(std::numeric_limits<double>::quiet_NaN())};
  Value out;
  std::string error;
  REQUIRE_FALSE(clpp::stdlib::std_apply(11, args, 2, out, error));
  args[1] = Value::number_of(std::numeric_limits<double>::infinity());
  REQUIRE_FALSE(clpp::stdlib::std_apply(11, args, 2, out, error));
  args[1] = Value::integer_of(3);
  REQUIRE(clpp::stdlib::std_apply(11, args, 2, out, error));
  REQUIRE(out.text == "aaa");
}

TEST_CASE("JSON indentation rejects overflowing native conversion", "[regression][native-text]") {
  using clpp::Value;
  const auto native = clpp::stdlib::host_native("json::Stringify", 2);
  REQUIRE(native >= 2000);
  Value args[] = {Value::struct_of({Value::integer_of(1)}), Value::integer_of(9223372036854775807LL)};
  Value out;
  std::string error;
  REQUIRE_FALSE(clpp::stdlib::host_apply(static_cast<std::uint8_t>(native - 2000), args, 2, out, error));
  REQUIRE(error == "Json.Stringify: indentation is outside the native integer range");
  args[1] = Value::integer_of(2);
  REQUIRE(clpp::stdlib::host_apply(static_cast<std::uint8_t>(native - 2000), args, 2, out, error));
  REQUIRE(out.text == "[\n  1\n]");
}

TEST_CASE("for-in over array<T> types the item, so its fields resolve", "[regression]") {
  const Run result = run(
      "def Enemy { string name; number hp; }\n"
      "array<Enemy> wave = array<Enemy>(Enemy { name = \"slime\", hp = 10 }, Enemy { name = \"orc\", hp = 40 });\n"
      "for (auto e in wave) { post(e.name .: \" \" .: e.hp); }\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "slime 10\norc 40\n");
}

TEST_CASE("link without `as` names the module after its last name; module types work qualified", "[regression]") {
  const Run result = run(
      "link \"./combat.clp\";\n"
      "link @clpp.axiom;\n"
      "Combat.Fighter heal(Combat.Fighter f) { f.hp += 5; return f; }\n"
      "Combat.Fighter a = Combat.Fighter { name = \"Ada\", hp = Combat.MAX_HP };\n"
      "post(heal(a).hp);\n"
      "post(Axiom.Clamp(5, 0, 3));\n",
      {{"./combat.clp", "export const auto MAX_HP = 100;\nexport def Fighter { string name; number hp; }\n"}});
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "105\n3\n");
}

TEST_CASE("a module's own links resolve inside its functions; two paths to one module share its types", "[regression]") {
  const std::map<std::string, std::string> files = {
      {"./combat.clp", "export const auto MAX_HP = 100;\nexport def Fighter { string name; number hp; bool alive() { return @this.hp > 0; } }\nexport Fighter hit(Fighter f, number dmg) { f.hp -= dmg; return f; }\n"},
      {"./d.clp", "link \"./combat.clp\" as C2;\n"
      "export any twice() { return C2.MAX_HP * 2; }\n"
      "export C2.Fighter make() { return C2.Fighter { name = \"z\", hp = C2.MAX_HP }; }\n"},
  };
  const Run result = run(
      "link \"./combat.clp\" as Combat;\nlink \"./d.clp\" as D;\n"
      "post(D.twice() + Combat.MAX_HP);\n"
      "Combat.Fighter f = D.make();\npost(Combat.hit(f, 1).hp);\npost(f.alive());\n",
      files);
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "300\n99\ntrue\n");
}

TEST_CASE("a signal can connect a module function", "[regression]") {
  const Run result = run("link \"./hud.clp\" as Hud;\nsignal<number> damaged;\ndamaged~>Connect(Hud.onDamage);\ndamaged::Fire(15);\n",
                         {{"./hud.clp", "export void onDamage(number amount) { post(\"HUD: -\" .: amount); }\n"}});
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "HUD: -15\n");
}

TEST_CASE("Axiom vector basics: Dot, Cross, Length, Normalize", "[regression]") {
  const Run result = run(
      "link @clpp.axiom as Axiom;\n"
      "post(Axiom.Dot(Vector3(1, 2, 3), Vector3(4, 5, 6)));\n"
      "post(Axiom.Cross(Vector3(1, 0, 0), Vector3(0, 1, 0)));\n"
      "post(Axiom.Length(Vector3(3, 4, 0)));\n"
      "post(Axiom.Normalize(Vector3(0, 0, 5)));\n"
      "post(Axiom.Normalize(Vector3(0, 0, 0)));\n"
      "post(Axiom.IsEven(4));\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "32\n(0, 0, 1)\n5\n(0, 0, 1)\n(0, 0, 0)\ntrue\n");
}

TEST_CASE("vectors subtract and divide; Vector2 keeps two components through math", "[regression]") {
  const Run result = run(
      "Vector3 a = Vector3(6, 0, 8);\nVector3 b = Vector3(1, 1, 1);\npost(a - b);\npost(a / 2);\n"
      "post(Vector2(1, 2) * 3);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "(5, -1, 7)\n(3, 0, 4)\n(3, 6)\n");
}


TEST_CASE("typed and inferred declarations preserve mutability and inference", "[regression][declarations]") {
  const Run result = run(
      "const auto limit = 100; const number base = 2;\n"
      "auto count = 1; count += base; number total = count; total += limit;\n"
      "const auto name = \"CL++\"; const auto enabled = true;\n"
      "post(total); post(name); post(enabled);\n"
      "for (number i = 0; i < 3; i += 1) { count += i; }\n"
      "for (number item in list(4, 5)) { count += item; } post(count);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "103\nCL++\ntrue\n15\n");
}

TEST_CASE("constants reject writes and inference keeps its original type", "[regression][declarations]") {
  for (const char* source : {
      "const auto n = 1; n = 2;",
      "const number n = 1; n += 2;",
      "auto n = 1; n = \"text\";",
      "const number n = \"text\";",
      "array<number> values = list(1, 2); for (string item in values) { post(item); }",
      "dictionary<string, number> values = dictionary<string, number>(\"one\", 1); for (number key in values) { post(key); }"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("obsolete declarations are rejected instead of translated", "[regression][declarations]") {
  for (const char* source : {
      "let value = 1;", "let mut value = 1;", "let value: number = 1;",
      "const value = 1;", "const value: number = 1;", "auto value: number = 1;",
      "if (let value = 1) { post(value); }", "let (a, b) = list(1, 2);",
      "for (let item in list(1, 2)) { post(item); }"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}


TEST_CASE("typed iteration visits dictionary keys and string characters", "[regression][declarations]") {
  const Run result = run(
      "dictionary<string, number> values = dictionary<string, number>(\"one\", 1);\n"
      "for (string key in values) { post(key); }\n"
      "for (string ch in \"ab\") { post(ch); }\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "one\na\nb\n");
}


TEST_CASE("inference preserves properties returned by functions fields and indexing", "[regression][properties]") {
  const Run result = run(
      "def Stats { number Health; } def Player { Stats Data; }\n"
      "Player makePlayer() { return Player { Data = Stats { Health = 100 } }; }\n"
      "array<Player> makeTeam() { return array<Player>(makePlayer()); }\n"
      "auto player = makePlayer(); const auto saved = makePlayer();\n"
      "auto stats = player.Data; auto team = makeTeam(); auto first = team[0];\n"
      "post(player.Data.Health); post(saved.Data.Health); post(stats.Health); post(first.Data.Health);\n"
      "post(makeTeam()[0].Data.Health);\n"
      "array<array<Player>> groups = array<array<Player>>(team); auto member = groups[0][0];\n"
      "post(member.Data.Health);\n"
      "dictionary<string, Player> roster = dictionary<string, Player>(\"Ana\", player);\n"
      "auto selected = roster[\"Ana\"]; post(selected.Data.Health); post(roster[\"Ana\"].Data.Health);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "100\n100\n100\n100\n100\n100\n100\n100\n");
}

TEST_CASE("inference preserves properties across module boundaries", "[regression][properties]") {
  const Run result = run(
      "link \"./players.clp\" as Players; auto player = Players.create(); post(player.Health);\n",
      {{"./players.clp", "export def Player { number Health; } export Player create() { return Player { Health = 75 }; }\n"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "75\n");
}

TEST_CASE("property inference still rejects absent fields", "[regression][properties]") {
  const Run result = run(
      "def Player { number Health; } Player create() { return Player { Health = 75 }; }\n"
      "auto player = create(); post(player.Missing);\n");
  REQUIRE_FALSE(result.compiled);
  REQUIRE(result.error == "unknown field");
}


TEST_CASE("def named initialization uses declared defaults and field names", "[regression][def]") {
  const Run result = run(
      "def Player { string Name; number Health = 100; number Mana = 30; }\n"
      "auto ada = Player { Name = \"Ada\" };\n"
      "Player rui = Player { Health = 50, Name = \"Rui\" };\n"
      "post(ada.Name); post(ada.Health); post(ada.Mana);\n"
      "post(rui.Name); post(rui.Health); post(rui.Mana);\n"
      "def Mage : Player { number Power = 5; }\n"
      "auto mage = Mage { Name = \"Mage\" }; post(mage.Health); post(mage.Power);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "Ada\n100\n30\nRui\n50\n30\n100\n5\n");
}

TEST_CASE("def aliases unions and generic fields use the same declaration", "[regression][def]") {
  const Run result = run(
      "def Points = number; def State = \"idle\" | \"walking\";\n"
      "const Points points = 10; const State state = \"idle\";\n"
      "def Box<T> { T Value; } Box<number> count = Box<number> { Value = points };\n"
      "post(count.Value); post(state);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "10\nidle\n");
}

TEST_CASE("named initialization rejects missing duplicate unknown and mistyped fields", "[regression][def]") {
  for (const char* source : {
      "def Player { string Name; } auto player = Player {};",
      "def Player { number Health; } auto player = Player { Health = 1, Health = 2 };",
      "def Player { number Health; } auto player = Player { Missing = 1 };",
      "def Player { number Health; } auto player = Player { Health = \"bad\" };",
      "def Player { number Health; } auto player = Player { .Health = 1 };"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("def defaults use declaration scope across functions and modules", "[regression][def]") {
  const Run result = run(
      "const number BASE = 100; def Player { number Health = BASE; number Mana = initialMana(); }\n"
      "number initialMana() { return 30; }\n"
      "Player create(number BASE) { return Player {}; }\n"
      "auto player = create(5); post(player.Health); post(player.Mana);\n"
      "link \"./defaults.clp\" as Defaults; auto imported = Defaults.Imported {}; post(imported.Value);\n",
      {{"./defaults.clp", "export const number BASE = 70; export number initial() { return BASE; } export def Imported { number Value = initial(); }\n"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "100\n30\n70\n");
}

TEST_CASE("invalid and recursive defaults fail without constructing the type", "[regression][def]") {
  for (const char* source : {
      "def Player { number Health = \"bad\"; }",
      "def Player { number Health = local; } Player create(number local) { return Player {}; }",
      "def A { A Child = A {}; }",
      "def A { B Child = B {}; } def B { A Child = A {}; }"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("type-first functions support recursion generics modules and methods", "[regression][functions]") {
  const Run result = run(
      "number Add(number left, number right) { return left + right; }\n"
      "number Factorial(number n) { if (n <= 1) { return 1; } return n * Factorial(n - 1); }\n"
      "def Player { number Health = 10; number Read() { return @this.Health; } }\n"
      "Player Create(number health) { return Player { Health = health }; }\n"
      "array<Player> Team() { return array<Player>(Create(75)); }\n"
      "void Print(number n) { post(n); }\n"
      "Print(Add(2, 3)); Print(Factorial(5)); auto team = Team(); auto first = team[0]; Print(first.Read());\n"
      "link \"./typed.clp\" as Typed; Print(Typed.Double(4));\n",
      {{"./typed.clp", "export number Double(number n) { return n * 2; }\n"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "5\n120\n75\n8\n");
}

TEST_CASE("type-first functions check argument and result types", "[regression][functions]") {
  for (const char* source : {
      "number Bad() { return \"wrong\"; }",
      "number Missing() { post(1); }",
      "number Add(number n) { return n; } post(Add(\"wrong\"));",
      "number Add(number n) { return n; } post(Add());"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("method receivers support indexing fields factories and call chains", "[regression][postfix]") {
  const Run result = run(
      "def Player { number Health = 10; number Read() { return @this.Health; } Player Same() { return @this; } }\n"
      "def Holder { Player Item; }\n"
      "Player Create() { post(99); return Player { Health = 75 }; }\n"
      "array<Player> Team() { return array<Player>(Player { Health = 25 }); }\n"
      "auto team = Team(); post(team[0].Read()); post(Team()[0].Read());\n"
      "post(Create().Same().Read()); auto holder = Holder { Item = team[0] }; post(holder.Item.Read());\n"
      "post((team[0]).Read()); team[0].Read();\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "25\n25\n99\n75\n25\n25\n");
}

TEST_CASE("chained methods retain argument and visibility checking", "[regression][postfix]") {
  for (const char* source : {
      "def P { number Read(number x) { return x; } } post((P {}).Read(\"bad\"));",
      "def P { number Read(number x) { return x; } } post((P {}).Read());",
      "def P { number Read() { return 1; } } post((P {}).Missing());",
      "def P { private number Read() { return 1; } } post((P {}).Read());"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("brace initializers cannot call ordinary functions or builtins", "[regression][def]") {
  for (const char* source : {
      "number Create() { return 1; } auto value = Create {};",
      "auto value = list {};",
      "auto value = number {};"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("scoped enums and canonical match arms preserve enum identity", "[regression][scoped-enum]") {
  const Run result = run(
      "enum class Direction { Up, Down, Left, Right }\n"
      "Direction facing = Direction::Left;\n"
      "match (facing) { Direction::Up => post(1); Direction::Down => post(2); Direction::Left => { post(3); } Direction::Right => post(4); }\n"
      "Direction Next(Direction value) { return Direction::Right; }\n"
      "match (Next(facing)) { Direction::Right => post(5); _ => post(6); }\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n5\n");
}

TEST_CASE("scoped enum checking rejects invalid and incomplete cases", "[regression][scoped-enum]") {
  for (const char* source : {
      "enum class D { Left } D x = D.Left;",
      "enum class D { Left } D x = D::Missing;",
      "enum class D { Left } enum class E { Left } D x = E::Left;",
      "enum class D { Left } D x = 0;",
      "enum class D { Left, Right } match(D::Left) { D::Left => post(1); }",
      "enum class D { Left } match(D::Left) { Left => post(1); }",
      "def P { number X; } auto x = P::X;"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("optional stores values and distinct null without wrapper constructors", "[regression][optional]") {
  const Run result = run(
      "optional<number> value = null; post(value); post(value == null);\n"
      "value = 0; post(value == null); post(value != null); post(value);\n"
      "optional<number> Empty() { return null; } optional<number> Present() { return 12; }\n"
      "post(Empty()); post(Present());\n"
      "def Player { optional<string> Name = null; } auto player = Player {}; post(player.Name);\n"
      "player.Name = \"Ada\"; post(player.Name);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "null\ntrue\nfalse\ntrue\n0\nnull\n12\nnull\nAda\n");
}

TEST_CASE("omitted optional def fields default to null across modules and generics", "[regression][optional-fields]") {
  const Run result = run(
      "link \"./Human.clp\" as H;\n"
      "auto kartz = H.Human { name = \"Kartz\", age = 15 }; post(kartz.cpf == null);\n"
      "auto supplied = H.Human { name = \"Ada\", age = 20, cpf = 100000 }; post(supplied.cpf);\n"
      "def Maybe = optional<number>; def Local { Maybe first; optional<number> second = 7; }\n"
      "auto local = Local {}; post(local.first == null); post(local.second);\n"
      "def Box<T> { optional<T> value; } auto box = Box<string> {}; post(box.value == null);\n"
      "def Slot<T> { T value; } auto slot = Slot<optional<number>> {}; post(slot.value == null);\n"
      "def Base<T> { optional<T> inherited; } def Child<T> : Base<T> { string name; }\n"
      "auto child = Child<number> { name = \"child\" }; post(child.inherited == null);\n",
      {{"./Human.clp", "export def Human { string name; number age; optional<number> cpf; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  CHECK(result.output == "true\n100000\ntrue\n7\ntrue\ntrue\ntrue\n");
  for (const char* source : {
      "def Human { string name; optional<number> cpf; } auto h = Human {};",
      "def Slot<T> { T value; } auto slot = Slot<number> {};",
      "def Item { any value; } auto item = Item {};",
      "def Item { optional<number> value; } auto item = Item { value = \"bad\" };"}) {
    INFO(source);
    CHECK_FALSE(clpp::Compiler{}.compile(source).ok());
  }
}

TEST_CASE("optional negation tests null and false while keeping zero present", "[regression][optional-fields]") {
  const Run result = run(
      "def Human { optional<number> cpf; } auto person = Human {}; post(!person.cpf);\n"
      "person.cpf = 0; post(!person.cpf); person.cpf = 10; post(!person.cpf);\n"
      "optional<bool> flag = null; post(!flag); flag = false; post(!flag); flag = true; post(!flag);\n"
      "optional<string> text = \"\"; post(!text); optional<number> zero = 0;\n"
      "if (zero != null) { post(!zero); }\n"
      "number count = 0; optional<number> Read() { count += 1; return null; } post(!Read()); post(count);\n"
      "template <typename T> bool Empty(optional<T> value) { return !value; }\n"
      "post(Empty<number>(0)); post(Empty<bool>(false));\n"
      "number Present(optional<number> value) { guard (value) else { return -1; } return value; }\n"
      "post(Present(null)); post(Present(0)); post(!false); post(!true);\n");
  INFO(result.error);
  INFO(result.detail);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  CHECK(result.output == "true\nfalse\nfalse\ntrue\ntrue\nfalse\nfalse\nfalse\ntrue\n1\nfalse\ntrue\n-1\n0\ntrue\nfalse\n");
}

TEST_CASE("optional conditions agree across guards loops and short circuit expressions", "[regression][optional-fields]") {
  const Run result = run(
      "def Human { optional<number> cpf; }\n"
      "bool Create(Human target) { guard (target.cpf) else { return false; } return true; }\n"
      "post(Create(Human {})); post(Create(Human { cpf = 0 }));\n"
      "optional<bool> flag = false; if (flag) { post(1); } else { post(2); }\n"
      "optional<number> zero = 0; if (zero) { post(zero + 3); }\n"
      "number Read(optional<number> value) { guard(value) else { return -1; } post(!value); return value; }\n"
      "post(Read(0)); post(Read(null));\n"
      "number calls = 0; optional<number> Next() { calls += 1; return 0; }\n"
      "post(flag and Next()); post(zero or Next()); post(calls);\n"
      "post(Next() and zero); post(calls);\n"
      "optional<number> loop = 0; while(loop) { post(4); loop = null; }\n"
      "optional<string> text = \"\"; if(text) { post(5); }\n"
      "post(zero ? 6 : 7); post(flag ? 8 : 9);\n");
  INFO(result.error);
  INFO(result.detail);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  CHECK(result.output == "false\ntrue\n2\n3\nfalse\n0\n-1\nfalse\ntrue\n0\ntrue\n1\n4\n5\n6\n9\n");
  for (const char* source : {
      "number Read(optional<number> value) { if(value) { value = null; return value + 1; } return 0; }",
      "number Read(optional<number> value) { if(!value) { return value + 1; } return 0; }",
      "number Read(optional<number> value) { guard(value) else { return 0; } value = null; return value + 1; }"}) {
    INFO(source);
    CHECK_FALSE(clpp::Compiler{}.compile(source).ok());
  }
}

TEST_CASE("postfix is tests imported nominal types and narrows short circuit operands", "[regression][type-test]") {
  const Run result = run(
      "link \"./Human.clp\" as H; any kartz = H.Human { name = \"Kartz\", age = 18 };\n"
      "post(kartz:is<H.Human> and kartz.cpf == null);\n"
      "any other = 0; post(other:is<H.Human> and other.cpf == null);\n"
      "number Age(any value) { guard(value:is<H.Human>) else { return -1; } return value.age; }\n"
      "post(Age(kartz)); post(Age(false));\n"
      "def Robot { string name; number age; optional<number> cpf; }\n"
      "any robot = Robot { name = \"Robot\", age = 18 }; post(robot:is<H.Human>);\n"
      "def Child : H.Human { number rank = 1; } any child = Child { name = \"Child\", age = 9 };\n"
      "post(child:is<H.Human>); post(child:is<Child>); post(kartz:is<Child>);\n"
      "def Person = H.Human; post(kartz:is<Person>);\n",
      {{"./Human.clp", "export def Human { string name; number age; optional<number> cpf; }"}});
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "true\nfalse\n18\n-1\nfalse\ntrue\ntrue\nfalse\ntrue\n");
}

TEST_CASE("postfix is distinguishes primitive types and evaluates its operand once", "[regression][type-test]") {
  const std::string source =
      "any value = 0; post(value:is<number>); post(value:is<float>); post(value:is<bool>);\n"
      "any decimal = 0.0; post(decimal:is<float>); post(decimal:is<number>);\n"
      "any flag = false; post(flag:is<bool>); post(flag:is<number>);\n"
      "any text = \"\"; post(text:is<string>); post(text:is<number | string>);\n"
      "any absent = null; post(absent:is<optional<number>>); post(absent:is<any>);\n"
      "number calls = 0; any Read() { calls += 1; return 1; } post(Read():is<number>); post(calls);\n"
      "def Mixed = number | string; number Code(Mixed item) { if(item:is<number>) { return item + 1; } return 0; }\n"
      "post(Code(2)); post(Code(\"text\"));\n"
      "post(false:is<bool>); post(null:is<optional<number>>); post(\"x\":is<string>); post(1:is<number>);\n";
  const Run result = run(source);
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "true\nfalse\nfalse\ntrue\nfalse\ntrue\nfalse\ntrue\ntrue\ntrue\ntrue\ntrue\n1\n3\n0\ntrue\ntrue\ntrue\ntrue\n");
  const auto compiled = clpp::Compiler{}.compile(source);
  const auto restored = clpp::deserialize_clir(clpp::serialize_clir(compiled.chunk));
  clpp::VirtualMachine vm; std::ostringstream output; vm.set_output(output); vm.load(restored);
  REQUIRE(vm.run()); CHECK(output.str() == result.output);
}

TEST_CASE("postfix is rejects malformed unknown and erased type tests", "[regression][type-test]") {
  for (const char* source : {
      "any value = 1; post(value:is);", "any value = 1; post(value:is<>);",
      "any value = 1; post(value:is<Missing>);", "any value = 1; post(value:is<void>);",
      "any value = 1; post(value:is<number>());",
      "def Box<T> { T value; } any value = Box<number> { value = 1 }; post(value:is<Box<number>>);",
      "any value = 1; post(value:is<array<number>>);",
      "signal<number> changed; post(changed:is<signal<number>>);",
      "signal<> changed; post(changed:is<signal<>>);",
      "enum class State { Ready } any value = State::Ready; post(value:is<State>);",
      "number Read(optional<number> value) { if(value:is<number>) { value = null; return value + 1; } return 0; }",
      "number Read(optional<number> value) { if(!value:is<number>) { return value + 1; } return 0; }"}) {
    INFO(source);
    CHECK_FALSE(clpp::Compiler{}.compile(source).ok());
  }
}

TEST_CASE("optional rejects incompatible types and unsafe use before narrowing", "[regression][optional]") {
  for (const char* source : {
      "number value = null;", "optional<number> value = \"bad\";",
      "optional<number> value = null; number result = value;",
      "optional<number> value = null; post(value + 1);",
      "optional<void> value = null;"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("null checks and guards narrow optional variables", "[regression][optional]") {
  const Run result = run(
      "def Player { number Health = 75; }\n"
      "void Read(optional<Player> player) { guard (player != null) else { return; } post(player.Health); }\n"
      "number Increment(optional<number> value) { guard (value != null) else { return 0; } return value + 1; }\n"
      "optional<number> score = 0; if (score != null) { number value = score; post(value + 2); }\n"
      "optional<Player> player = Player {}; if (player == null) { post(0); } else { post(player.Health); }\n"
      "Read(player); Read(null); post(Increment(10)); post(Increment(null));\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "2\n75\n75\n11\n0\n");
}

TEST_CASE("guards require exit and writes invalidate optional refinement", "[regression][optional]") {
  for (const char* source : {
      "void Read(optional<number> x) { guard (x != null) else { post(0); } post(x + 1); }",
      "void Read(optional<number> x) { guard (x != null) else { return; } x = null; post(x + 1); }",
      "void Read(optional<number> x) { if (x != null) { post(x + 1); } post(x + 1); }",
      "void Read(optional<number> x) { guard (x != null) else { return; } if (true) { x = null; } post(x + 1); }"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("optional checks narrow short circuit operands", "[regression][optional]") {
  const Run result = run(
      "def Player { number Health = 75; } optional<Player> player = null;\n"
      "post(player != null and player.Health > 0); post(player == null or player.Health > 0);\n"
      "player = Player {}; post(player != null and player.Health > 0);\n"
      "void Read(optional<Player> value) { guard (value != null and value.Health > 0) else { return; } post(value.Health); }\n"
      "Read(player); Read(null);\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "false\ntrue\ntrue\n75\n");
}

TEST_CASE("literal unions and guarded patterns require complete coverage", "[regression][match-contract]") {
  const Run result = run(
      "def State = \"idle\" | \"walking\";\n"
      "number Code(State state) { match (state) { \"idle\" => { return 1; } \"walking\" => { return 2; } } }\n"
      "post(Code(\"walking\")); number Number(number x) { match(x) { 1 => { return 10; } _ => { return 20; } } } post(Number(9));\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "2\n20\n");
}

TEST_CASE("match rejects duplicate missing and unreachable cases", "[regression][match-contract]") {
  for (const char* source : {
      "enum class D { Left } match(D::Left) { D::Left => post(1); D::Left => post(2); }",
      "enum class D { Left } match(D::Left) { _ guard false => post(1); }",
      "def S = \"a\" | \"b\"; S x = \"a\"; match(x) { \"a\" => post(1); }",
      "def S = \"a\" | \"b\"; S x = \"a\"; match(x) { \"c\" => post(1); _ => post(2); }",
      "def S = \"a\" | \"b\"; S x = \"a\"; match(x) { \"a\" => post(1); \"a\" => post(2); \"b\" => post(3); }",
      "number Code(number x) { match(x) { 1 => { return 10; } } }",
      "match(1) { _ => post(1); 1 => post(2); }"}) {
    INFO(source);
    const clpp::CompileResult result = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
  }
}

TEST_CASE("header imports are rejected before consulting the loader", "[regression][modules]") {
  for (const char* path : {"./player.clh", "./player.h", "./player.hpp"}) {
    int loads = 0;
    const std::string source = std::string("link \"") + path + "\" as Player;";
    const clpp::CompileResult result = clpp::Compiler{}.compile(source, [&](std::string_view) -> std::optional<std::string> {
      ++loads;
      return "def Player { number Health = 100; }";
    });
    INFO(path);
    REQUIRE_FALSE(result.ok());
    REQUIRE(result.chunk.code.empty());
    REQUIRE(loads == 0);
  }
}

TEST_CASE("type-first async functions support spawn await and parallel", "[regression][async]") {
  const Run result = run(
      "async number Compute(number value) { return value * 2; }\n"
      "task<number> job = spawn Compute(21); post(await job);\n"
      "auto results = parallel(Compute(2), Compute(5)); post(results[0]); post(results[1]);\n"
      "number Work(number value) { return value; } auto first = thread Work(20); auto second = thread Work(22);\n"
      "post(join(first) + join(second));\n");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "42\n4\n10\n42\n");
}

TEST_CASE("unused functions still reject unknown return types", "[regression][functions]") {
  const clpp::CompileResult result = clpp::Compiler{}.compile("Missing Stop() { report(\"stop\"); }");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.chunk.code.empty());
}

TEST_CASE("nested module links resolve relative to their importing file", "[regression][modules]") {
  const Run result = run(
      "link \"./game/combat.clp\" as Combat; post(Combat.Damage());\n",
      {{"./game/combat.clp", "link \"./utils.clp\" as Utils; link \"../shared/bonus.clp\" as Bonus; export number Damage() { return Utils.Base() + Bonus.Value(); }"},
       {"./game/utils.clp", "export number Base() { return 10; }"},
       {"./shared/bonus.clp", "export number Value() { return 5; }"},
       {"./utils.clp", "export number Base() { return 999; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "15\n");
}

TEST_CASE("normalized nested paths detect circular links", "[regression][modules]") {
  const Run result = run(
      "link \"./game/main.clp\" as Game;",
      {{"./game/main.clp", "link \"./sub/../main.clp\" as Again; export number Value() { return 1; }"}});
  REQUIRE_FALSE(result.compiled);
  REQUIRE(result.error.find("circular link") != std::string::npos);
}

TEST_CASE("module qualified annotations resolve through their actual alias", "[regression][modules]") {
  const auto files = std::map<std::string, std::string>{{"./data.clp", "export def Player { number Health = 75; } export Player Make() { return Player {}; }"}};
  const Run valid = run(
      "link \"./data.clp\" as Data; Data.Player Read(Data.Player p) { return p; }\n"
      "def Wrapper { optional<Data.Player> Item; } Data.Player p = Read(Data.Make());\n"
      "post(p.Health); def Label = \"Data.Player\"; Label label = \"Data.Player\"; post(label);", files);
  INFO(valid.error);
  REQUIRE(valid.compiled);
  REQUIRE(valid.ran);
  REQUIRE(valid.output == "75\nData.Player\n");
  for (const char* declaration : {"Wrong.Player Read() { return Data.Make(); }",
       "void Read(Wrong.Player p) {}", "def Wrapper { Wrong.Player Item; }",
       "def Hidden = Wrong.Player; Hidden p = Data.Make();"}) {
    INFO(declaration);
    const Run invalid = run(std::string("link \"./data.clp\" as Data; ") + declaration, files);
    REQUIRE_FALSE(invalid.compiled);
  }
}

TEST_CASE("different modules keep nominal data and enum types independent", "[regression][modules]") {
  const std::map<std::string, std::string> files = {
      {"./left.clp", "export def Player { number Health = 10; number Read() { return @this.Health; } } export enum class Mode { Ready }"},
      {"./right.clp", "export def Player { number Health = 20; number Read() { return @this.Health; } } export enum class Mode { Ready }"}};
  const std::string imports = "link \"./left.clp\" as Left; link \"./right.clp\" as Right; ";
  const Run good = run(imports +
      "Left.Player a = Left.Player {}; Right.Player b = Right.Player {}; post(a.Read()); post(b.Read()); "
      "Left.Mode mode = Left.Mode::Ready; match(mode) { Left.Mode::Ready => post(30); }", files);
  INFO(good.error);
  REQUIRE(good.compiled);
  REQUIRE(good.ran);
  REQUIRE(good.output == "10\n20\n30\n");
  REQUIRE_FALSE(run(imports + "Left.Player a = Right.Player {};", files).compiled);
  REQUIRE_FALSE(run(imports + "Left.Mode mode = Right.Mode::Ready;", files).compiled);
  REQUIRE_FALSE(run(imports + "Player value = Left.Player {};", files).compiled);
}

TEST_CASE("aliases of one normalized module share data identity", "[regression][modules]") {
  const Run result = run(
      "link \"./data.clp\" as First; link \"./sub/../data.clp\" as Second; "
      "First.Player p = Second.Player { Health = 90 }; Second.Player q = p; post(q.Read()); "
      "First.Box<number> box = Second.Box<number> { Value = 12 }; post(box.Value);",
      {{"./data.clp", "export def Player { number Health; number Read() { return @this.Health; } } export def Box<T> { T Value; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "90\n12\n");
}

TEST_CASE("module type resolution respects generic and local names", "[regression][modules]") {
  const Run result = run("link \"./data.clp\" as Data; post(Data.Read(3)); post(Data.Local());",
      {{"./data.clp", "export def Player { number Health; } export number Read(number Player) { return Player; } export number Local() { number Player = 4; return Player; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.output == "3\n4\n");
}

TEST_CASE("nested modules isolate matching function aliases and helper names", "[regression][modules]") {
  const Run result = run(
      "link \"./one/main.clp\" as One; link \"./two/main.clp\" as Two; "
      "post(One.Read()); post(Two.Read());",
      {{"./one/main.clp", "link \"./utils.clp\" as Utils; export number Helper() { return 1; } export number Read() { return Utils.Read() + Helper(); }"},
       {"./two/main.clp", "link \"./utils.clp\" as Utils; export number Helper() { return 2; } export number Read() { return Utils.Read() + Helper(); }"},
       {"./one/utils.clp", "export number Read() { return 10; }"}, {"./two/utils.clp", "export number Read() { return 20; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "11\n22\n");
}

TEST_CASE("shared nested dependencies reuse functions and never leak local aliases", "[regression][modules]") {
  const Run result = run(
      "link \"./one.clp\" as One; link \"./two.clp\" as Two; post(One.Read()); post(Two.Read());",
      {{"./one.clp", "link \"./shared.clp\" as Dependency; export number Read() { return Dependency.Read(); }"},
       {"./two.clp", "link \"./shared.clp\" as Dependency; export number Read() { return Dependency.Read(); }"},
       {"./shared.clp", "export number Read() { return 42; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.output == "42\n42\n");
  REQUIRE_FALSE(run("link \"./one.clp\" as One; post(Dependency.Read());",
      {{"./one.clp", "link \"./shared.clp\" as Dependency; export number Read() { return Dependency.Read(); }"},
       {"./shared.clp", "export number Read() { return 42; }"}}).compiled);
}


TEST_CASE("JSON preserves null separately from numeric zero in nested values", "[regression][json]") {
  const Run result = run(R"CLPP(
link @clpp.json as Json;
post(Json.Parse("null"));
post(Json.Stringify(Json.Parse("null")));
post(Json.Stringify(Json.Parse("[null,0,\"null\"]")));
post(Json.Stringify(Json.Parse("{\"value\":null,\"zero\":0}")));
post(Json.Stringify(null));
post(Json.Stringify(0));
)CLPP");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "null\nnull\n[null,0,\"null\"]\n{\"value\":null,\"zero\":0}\nnull\n0\n");
}


TEST_CASE("JSON numbers follow decimal grammar and reject binary64 overflow", "[regression][json]") {
  for (const std::string text : {"01", "-01", ".1", "1.", "+1", "1e", "1e+", "1e9999", "1-2", "NaN", "Infinity"}) {
    const auto result = run("link @clpp.json as Json; post(Json.Valid(\"" + text + "\"));");
    INFO(text);
    INFO(result.error);
    REQUIRE(result.ran);
    REQUIRE(result.output == "false\n");
  }
  for (const std::string text : {"0", "-0", "1", "-12", "1.25", "1e2", "1E-2", "-1.25e+2"}) {
    const auto result = run("link @clpp.json as Json; post(Json.Valid(\"" + text + "\"));");
    INFO(text);
    INFO(result.error);
    REQUIRE(result.ran);
    REQUIRE(result.output == "true\n");
  }
}

TEST_CASE("JSON paths reject overflowing indexes without a native exception", "[regression][json]") {
  const auto result = run(R"CLPP(
link @clpp.json as Json;
auto data = Json.Parse("[null,42]");
post(Json.Has(data, "0"));
post(Json.Has(data, "999999999999999999999999999999999999999999999"));
post(Json.Has(data, "-1"));
post(Json.Has(data, "1.0"));
post(Json.Get(data, "1"));
try { Json.Get(data, "999999999999999999999999999999999999999999999"); }
catch (e) { post("missing"); }
try { Json.Set(data, 0.5, 7); } catch (e) { post("invalid index"); }
)CLPP");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "true\nfalse\nfalse\nfalse\n42\nmissing\ninvalid index\n");
}


TEST_CASE("console module keeps output streams and typed queries after declaration migration", "[regression][console-module]") {
  struct Capture {
    std::ostringstream output;
    std::ostringstream errors;
    std::streambuf* previous_output = std::cout.rdbuf(output.rdbuf());
    std::streambuf* previous_errors = std::cerr.rdbuf(errors.rdbuf());
    ~Capture() {
      std::cout.rdbuf(previous_output);
      std::cerr.rdbuf(previous_errors);
    }
  };
  Run result;
  std::string output;
  std::string errors;
  {
    Capture capture;
    result = run(R"CLPP(
link @clpp.io as Io;
Io.Print("value=");
Io.Print(7);
Io.Print("\n");
Io.Print(null);
Io.Print("\n");
Io.PrintError("issue");
post(Io.Columns() > 0);
post(Io.Rows() > 0);
bool terminal = Io.IsTerminal();
post("ok");
)CLPP");
    output = capture.output.str();
    errors = capture.errors.str();
  }
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(output == "value=7\nnull\n");
  REQUIRE(errors == "issue");
  REQUIRE(result.output == "true\ntrue\nok\n");
  REQUIRE_FALSE(run("link @clpp.io as Io; Io.Color(123);").compiled);
  REQUIRE_FALSE(run("link @clpp.io as Io; Io.MoveTo(\"x\", 1);").compiled);
}


TEST_CASE("input module preserves explicit typed calls and actor isolation", "[regression][input-module]") {
  // Type-check every wrapper without running desktop automation.
  const auto result = run(R"CLPP(
link @clpp.input as Input;
void CheckCalls() {
  Input.Move(1, 2); Input.MoveBy(1, 2);
  number x = Input.MouseX(); number y = Input.MouseY();
  Input.Click(0); Input.Click(1); Input.DoubleClick(0); Input.DoubleClick(2);
  Input.MouseDown(0); Input.MouseDown(1); Input.MouseUp(0); Input.MouseUp(2);
  Input.ClickAt(1, 2, 0); Input.ClickAt(1, 2, 1);
  Input.Scroll(1); Input.Press("enter"); Input.KeyDown("a"); Input.KeyUp("a");
  Input.Hotkey("ctrl+s"); Input.Type("text");
  bool down = Input.IsDown("a"); number color = Input.Pixel(1, 2);
  number width = Input.ScreenWidth(); number height = Input.ScreenHeight();
  number image = Input.Capture(1, 2, 3, 4); Input.Wait(0);
}
Input.Wait(0);
post("ok");
)CLPP");
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "ok\n");
  for (const char* invalid : {"Input.Move(\"x\", 2);", "Input.Click(\"left\");", "Input.Press(1);",
                              "Input.Capture(1, 2, 3);", "string width = Input.ScreenWidth();"}) {
    INFO(invalid);
    REQUIRE_FALSE(run(std::string("link @clpp.input as Input; ") + invalid).compiled);
  }
  const auto blocked = run("link @clpp.input as Input; number Probe(number n) { Input.Wait(0); return n; } post(actor(Probe, 1));");
  INFO(blocked.error);
  REQUIRE(blocked.compiled);
  REQUIRE_FALSE(blocked.ran);
  REQUIRE(blocked.error == "sandbox");
}


TEST_CASE("generic union branches infer unique evidence and preserve concrete alternatives", "[generic-composition]") {
  const auto result = run(R"(
    def InputOf<T> = T|string;
    def SequenceOf<T> = array<T>|string;
    template <typename T> T Choose(InputOf<T> value, T fallback) { return fallback; }
    template <typename T> T Read(SequenceOf<T> value, T fallback) { return fallback; }
    def Input = number|string;
    Input value = 7;
    post(Choose(value, 42));
    post(Choose("ignored", 9));
    array<number> numbers = {1, 2};
    post(Read(numbers, 3));
    post(Read("ignored", 4));
  )");
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n9\n3\n4\n");
}

TEST_CASE("generic intersections infer evidence while checking every nominal contract", "[generic-composition]") {
  const auto result = run(R"(
    abstract def Readable { abstract number Read(); }
    def Box<T> : Readable { T Value; number Read() { return 7; } }
    def ReadableBox<T> = Box<T>&Readable;
    template <typename T> T Read(ReadableBox<T> value) { return value.Value; }
    post(Read(Box<number> { Value = 42 }));
    post(Read(Box<string> { Value = "ready" }));
  )");
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\nready\n");
}

TEST_CASE("generic composed arguments resolve later evidence and contextual callbacks", "[generic-composition][contextual-callback]") {
  const auto result = run(R"(
    def Either<T,U> = T|U;
    def Transform<T> = (T)->T;
    def NumberOrFlag = number|bool;
    template <typename T, typename U>
    T Pick(Either<T,U> value, T fallback, U witness) { return fallback; }
    template <typename T, typename U>
    T Apply(Either<T,U> value, Transform<T> transform, T initial, U witness) { return transform(initial); }
    NumberOrFlag primary = true;
    post(Pick(true, 7, false));
    post(Pick(1, 8, false));
    post(Pick(primary, 9, false));
    post(Apply(true, func(value) -> number { return value + 1; }, 41, false));
  )");
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\n8\n9\n42\n");
}

TEST_CASE("generic composition rejects missing ambiguous and conflicting evidence", "[generic-composition]") {
  for (const auto& source : {
    "def Input<T> = T|string; template <typename T> number Missing(Input<T> value) { return 1; } post(Missing(\"ignored\"));",
    "def Either<T,U> = T|U; template <typename T, typename U> number Ambiguous(Either<T,U> value) { return 1; } post(Ambiguous(7));",
    "def Input<T> = T|string; template <typename T> T Choose(Input<T> value, T fallback) { return fallback; } post(Choose(7, true));",
    "def Input = number|bool; def Choice<T> = T|string; template <typename T> T Choose(Choice<T> value, T fallback) { return fallback; } Input value=7; post(Choose(value, 1));",
    "abstract def Readable { abstract number Read(); } def Box<T> { T Value; } def ReadableBox<T> = Box<T>&Readable; template <typename T> T Read(ReadableBox<T> value) { return value.Value; } post(Read(Box<number> { Value=7 }));"
  }) {
    INFO(source); const auto result = run(source); CHECK_FALSE(result.compiled);
    CHECK(result.error.find("expected") == std::string::npos);
  }
}

TEST_CASE("rejected union candidates do not leak partial generic evidence", "[generic-composition]") {
  const auto result = run(R"(
    def Pair<T,U> { T First; U Second; }
    def Input<T> = Pair<T,number>|Pair<number,T>;
    template <typename T> T Read(Input<T> value, T fallback) { return fallback; }
    Pair<number,string> pair = Pair<number,string> { First = 7, Second = "ready" };
    post(Read(pair, "ok"));
  )");
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "ok\n");
}

TEST_CASE("union assignments accept every source alternative and reject partial coverage", "[regression][union-assignment]") {
  const auto accepted = run(R"CLPP(
def Small = "idle" | "running";
def Wide = "idle" | "running" | "jumping";
def Reordered = "running" | "idle";
def Numeric = number | float;
def Data = string | float | number;
Wide Widen(Small value) { Wide copy = value; return copy; }
Wide Echo(Wide value) { return value; }
Small Reorder(Reordered value) { return value; }
string Text(Small value) { return value; }
Data Payload(Numeric value) { return value; }
float Number(Numeric value) { return value; }
Small state = "idle";
Wide expanded = Widen(state);
post(expanded);
post(Echo(state));
post(Reorder("running"));
post(Text(state));
post(Payload(7));
post(Payload(2.5));
post(Number(3) / 2);
)CLPP");
  INFO(accepted.error);
  REQUIRE(accepted.compiled);
  REQUIRE(accepted.ran);
  REQUIRE(accepted.output == "idle\nidle\nrunning\nidle\n7\n2.5\n1.5\n");
  for (const char* source : {
      "def Small = number | string; def Wide = number | string | bool; Small Narrow(Wide value) { return value; }",
      "def Value = number | string; float Number(Value value) { return value; }",
      "def Value = number | string; string Text(Value value) { return value; }",
      "def Value = number | bool; float Number(Value value) { return value; }"}) {
    INFO(source);
    REQUIRE_FALSE(run(source).compiled);
  }
}


TEST_CASE("mutable generic types do not erase or widen their type arguments", "[regression][generic-compatibility]") {
  for (const char* source : {
      "array<number> a = array<number>(1); array<string> b = a;",
      "array<number> a = array<number>(1); array<float> b = a;",
      "array<array<number>> a = array<array<number>>(array<number>(1)); array<array<string>> b = a;",
      "dictionary<string, number> a = dictionary<string, number>(\"x\", 1); dictionary<number, number> b = a;",
      "dictionary<string, number> a = dictionary<string, number>(\"x\", 1); dictionary<string, float> b = a;",
      "def Box<T> { T Value; } Box<number> a = Box<number> { Value = 1 }; Box<string> b = a;",
      "def Box<T> { T Value; } Box<number> a = Box<number> { Value = 1 }; Box<float> b = a;",
      "def Box<T> { T Value; } Box<number> a = Box<number> { Value = 1 }; Box b = a;",
      "void Use(array<string> values) {} Use(array<number>(1));",
      "array<string> Wrong(array<number> values) { return values; }",
      "array<string> Wrong() { return array<number>(1); }",
      "auto values = array<number>(1); array<string> other = values;",
      "array<number> values = array<number>(\"bad\");",
      "dictionary<string, number> values = dictionary<string, number>(1, 2);",
      "dictionary<string, number> values = dictionary<string, number>(\"key\", \"bad\");",
      "auto values = dictionary<string, number>(\"key\");",
      "auto values = array<Missing>();"}) {
    INFO(source);
    REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Box<T> { T Value; }
def Small = "idle" | "running";
def Reordered = "running" | "idle";
def Base { number Health; }
def Child : Base { number Mana; }
array<number> Copy(array<number> values) { return values; }
array<number> numbers = Copy(array<number>(1, 2));
post(numbers[1]);
array<array<number>> nested = array<array<number>>(numbers);
array<array<number>> same = nested;
post(same[0][0]);
dictionary<string, number> original = dictionary<string, number>("score", 7);
dictionary<string, number> copied = original;
post(copied["score"]);
Box<number> box = Box<number> { Value = 12 };
Box<number> other = box;
post(other.Value);
array<Small> states = array<Small>("idle");
array<Reordered> equivalent = states;
post(equivalent[0]);
Child child = Child { Health = 100, Mana = 30 };
Base base = child;
post(base.Health);
)CLPP");
  INFO(valid.error);
  REQUIRE(valid.compiled);
  REQUIRE(valid.ran);
  REQUIRE(valid.output == "2\n1\n7\n12\nidle\n100\n");
}


TEST_CASE("generic data definitions require the declared type argument count", "[regression][generic-arity]") {
  for (const char* source : {
      "def Box<T> { T Value; } Box<number, string> box = Box<number, string> { Value = 1 };",
      "def Box<T> { T Value; } Box box = Box { Value = 1 };",
      "def Pair<A, B> { A First; B Second; } Pair<number> pair = Pair<number> { First = 1, Second = 2 };",
      "def Point { number X; } auto point = Point<number> { X = 1 };",
      "def Empty<T> {} auto value = Empty<number, string> {};",
      "def Empty<T> {} auto value = Empty {};",
      "def Box<T> { T Value; } Box<Missing> box = Box<Missing> { Value = 1 };",
      "def Box<T> { T Value; } void Use(Box<number, string> value) {}",
      "def Box<T> { T Value; } Box<number, string> Get() { return Box<number> { Value = 1 }; }",
      "def Box<T> { T Value; } def Holder { Box<number, string> Item; }",
      "array<array<Missing>> values = array<array<Missing>>();",
      "dictionary<string, array<Missing>> values = dictionary<string, array<Missing>>();"}) {
    INFO(source);
    REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Holder { Box<number> Item; }
def Pair<A, B> { A First; B Second; }
def Box<T> { T Value; }
Box<number> Create() { return Box<number> { Value = 5 }; }
Holder holder = Holder { Item = Create() };
post(holder.Item.Value);
Pair<string, number> pair = Pair<string, number> { First = "score", Second = 12 };
post(pair.First);
post(pair.Second);
array<Box<number>> values = array<Box<number>>(Create());
post(values[0].Value);
)CLPP");
  INFO(valid.error);
  REQUIRE(valid.compiled);
  REQUIRE(valid.ran);
  REQUIRE(valid.output == "5\nscore\n12\n5\n");
}

TEST_CASE("generic fields retain concrete types through nested access and construction", "[regression][generic-fields]") {
  for (const char* source : {
      "def Box<T> { T Value; } auto b = Box<number> { Value = 1 }; string s = b.Value;",
      "def Box<T> { T Value; } auto b = Box<number> { Value = 1 }; b.Value = \"bad\";",
      "def Box<T> { array<T> Items; } auto b = Box<number> { Items = array<string>(\"bad\") };",
      "def Box<T> { array<T> Items; } auto b = Box<number> { Items = array<number>(1) }; string s = b.Items[0];",
      "def Box<T> { array<T> Items; } auto b = Box<number> { Items = array<number>(1) }; b.Items = array<string>();",
      "def Box<T> { T Value = 1; } auto b = Box<string> {};",
      "def Box<T> { array<Missing> Items; }",
      "def Box<T> { optional<T> Value; } auto b = Box<void> { Value = null };",
      "def Box<T> { array<T> Items; } auto b = Box<number> { Items = array<number>(1) }; b.Items[0] = \"bad\";",
      "def Box<T> { T Value; } number Read(Box<string> b) { return b.Value; }",
      "def Box<T> { dictionary<string, T> Items; } auto b = Box<number> { Items = dictionary<string, number>(\"x\", 1) }; b.Items[\"x\"] = \"bad\";",
      "def Box<T> { dictionary<string, T> Items; } auto b = Box<number> { Items = dictionary<string, string>(\"x\", \"bad\") };"}) {
    INFO(source);
    REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Player { number Health; }
def Box<T> { T Value; array<T> Items; dictionary<string, T> Named; }
def Nested<T> { Box<T> Inner; }
def Default<T> { T Value = 1; }
Box<Player> Create() {
  return Box<Player> { Value = Player { Health = 5 }, Items = array<Player>(Player { Health = 6 }), Named = dictionary<string, Player>("p", Player { Health = 7 }) };
}
auto nested = Nested<Player> { Inner = Create() };
Player value = nested.Inner.Value;
post(value.Health);
post(nested.Inner.Items[0].Health);
post(nested.Inner.Named["p"].Health);
nested.Inner.Value = Player { Health = 8 };
post(nested.Inner.Value.Health);
auto item = nested.Inner.Items[0];
post(item.Health);
auto defaults = Default<number> {};
post(defaults.Value + 2);
def Choice<T> { array<T | bool> Items; optional<T> Maybe; }
auto choice = Choice<number> { Items = array<number | bool>(1, true), Maybe = null };
array<number | bool> choices = choice.Items;
post(choices[0]);
def Quoted<T> { T Value; }
auto quoted = Quoted<"T,|<"> { Value = "T,|<" };
post(quoted.Value);
)CLPP");
  INFO(valid.error);
  REQUIRE(valid.compiled);
  REQUIRE(valid.ran);
  REQUIRE(valid.output == "5\n6\n7\n8\n6\n3\n1\nT,|<\n");
}

TEST_CASE("explicit generic functions substitute parameters results and body annotations", "[regression][generic-functions]") {
  for (const char* source : {
      "template <typename T> T Id(T value) { return value; } Id<number>(\"bad\");",
      "template <typename T> T Id(T value) { return value; } string text = Id<number>(1);",
      "template <typename T> T Id(T value) { return value; } Id<number, string>(1);",
      "template <typename T> T Id(T value) { return value; } Id<Missing>(1);",
      "template <typename T> optional<T> Empty() { return null; } Empty<void>();",
      "template <typename T> T Bad(T value) {}",
      "template <typename T> T Good(T value) { return value; } template <typename T> T Bad(T value) { return Good<number>(value); }",

      "number Id(number value) { return value; } Id<number>(1);",
      "template <typename T> T Bad(T value) { return 1; }",
      "template <typename T> T Bad(T value) { return value + 1; }",
      "template <typename T> array<T> Echo(array<T> value) { return value; } Echo<string>(array<number>(1));",
      "template <typename T> array<T> Echo(array<T> value) { return value; } array<string> text = Echo<number>(array<number>(1));",
      "template <typename T> T Wrong(T value) { string other = value; return value; }",
      "template <typename T> T Wrong(T value) { T other = 1; return other; }",
      "template <typename T, typename T> T Id(T value) { return value; }",
      "template <typename T : Missing> T Id(T value) { return value; }",
      "template <typename T : number> T Id(T value) { return value; } Id<string>(\"bad\");",
      "template <typename T> optional<T> Id(T value) { return value; } Id<void>(post(1));"}) {
    INFO(source);
    const auto result = run(source);
    REQUIRE_FALSE(result.compiled);
  }
  const auto valid = run(R"CLPP(
def Player { number Health; }
def Box<T> { T Value; }
template <typename T>
T Identity(T value) { T copy = value; return copy; }
template <typename T>
array<T> Echo(array<T> values) { return values; }
template <typename T>
T Read(Box<T> box) { return Identity<T>(box.Value); }
template <typename T>
optional<T> First(array<T> values) {
  guard (len(values) > 0) else { return null; }
  return values[0];
}
template <typename T>
async array<T> AsyncEcho(array<T> values) { return values; }
template <typename T : number>
T Number(T value) { return value; }
post(Identity<number>(5));
post(Identity<string>("ok"));
post(Identity<Player>(Player { Health = 6 }).Health);
auto player = Read<Player>(Box<Player> { Value = Player { Health = 7 } });
post(player.Health);
auto team = Echo<Player>(array<Player>(player));
post(team[0].Health);
optional<number> first = First(array<number>(8));
if (first != null) { post(first + 1); }
post(First<number>(array<number>()) == null);
auto asyncTeam = await AsyncEcho<Player>(array<Player>(player));
post(asyncTeam[0].Health);
post(Number<number>(10));
)CLPP");
  INFO(valid.error);
  REQUIRE(valid.compiled);
  REQUIRE(valid.ran);
  REQUIRE(valid.output == "5\nok\n6\n7\n7\n9\ntrue\n7\n10\n");
}

TEST_CASE("exported generic functions preserve concrete imported types", "[regression][generic-functions][modules]") {
  const auto result = run(
      "link \"./generic.clp\" as G; auto item = G.Identity<G.Player>(G.Player { Health = 11 }); post(item.Health);",
      {{"./generic.clp", "export def Player { number Health; } export template <typename T> T Identity(T value) { return value; }"}});
  INFO(result.error);
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  REQUIRE(result.output == "11\n");
  REQUIRE_FALSE(run(
      "link \"./generic.clp\" as G; G.Identity<number>(\"bad\");",
      {{"./generic.clp", "export template <typename T> T Identity(T value) { return value; }"}}).compiled);
}

TEST_CASE("generic calls infer concrete types without dynamic guesses", "[regression][generic-inference]") {
  for (const char* source : {
      "template <typename T> T Choose(T a, T b) { return a; } Choose(1, \"bad\");",
      "template <typename T> T Choose(T a, T b) { return a; } Choose(1, 2.5);",
      "template <typename T> optional<T> Empty() { return null; } Empty();",
      "template <typename T> T Id(T value) { return value; } Id(null);",
      "template <typename T> T Id(T value) { return value; } string text = Id(1);",
      "template <typename T : number> T Id(T value) { return value; } Id(\"bad\");",
      "template <typename T> T First(array<T> values) { return values[0]; } First(1);",
      "template <typename T> T First(array<T> values) { return values[0]; } string text = First(array<number>(1));",
      "template <typename T> T Id(T value) { return value; } void Test(any value) { Id(value); }",
      "template <typename T> T Id(T value) { return value; } Id(array<any>());",
      "template <typename T> T Read(optional<T> value) { guard (value != null) else { report(\"empty\"); } return value; } Read(null);",
      "template <typename T> T Read(optional<T> value) { guard (value != null) else { report(\"empty\"); } return value; } string text = Read(1);",
      "template <typename T, typename U> T Id(T value) { return value; } Id(1);"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Player { number Health; }
def Box<T> { T Value; }
template <typename T> T Identity(T value) { return value; }
template <typename T> T Nested(T value) { return Identity(value); }
template <typename T> T Read(Box<T> box) { return box.Value; }
template <typename T> T First(array<T> values) { return values[0]; }
template <typename K, typename V> dictionary<K, V> Echo(dictionary<K, V> values) { return values; }
template <typename T> async array<T> AsyncEcho(array<T> values) { return values; }
template <typename T> T Choose(T a, T b) { return a; }
post(Identity(5)); post(Identity("ok")); post(Choose("a", "b"));
auto player = Identity(Player { Health = 7 }); post(player.Health);
post(Identity(Player { Health = 8 }).Health);
auto box = Box<Player> { Value = player }; post(Read(box).Health);
auto team = array<Player>(player); post(First(team).Health);
auto table = Echo(dictionary<string, Player>("p", player)); post(table["p"].Health);
auto count = 2 + 3; post(Identity(count));
post(Nested(player).Health);
auto awaited = await AsyncEcho(team); post(awaited[0].Health);
post(Choose<float>(1, 2.5));
template <typename T> T ReadOptional(optional<T> value) {
  guard (value != null) else { report("empty"); }
  return value;
}
optional<number> optionalNumber = 12;
post(ReadOptional(optionalNumber));
post(ReadOptional(13));
enum class State { Idle, Ready }
auto state = State::Ready;
State copied = Identity(state); post(copied == State::Ready);
def Words = "left" | "right";
Words word = "left"; Words same = Identity(word); post(same);
array<Words> words = array<Words>("right"); post(First(words));
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "5\nok\na\n7\n8\n7\n7\n7\n5\n7\n7\n1\n12\n13\ntrue\nleft\nright\n");
}

TEST_CASE("inferred imported generic calls retain nominal module identities", "[regression][generic-inference][modules]") {
  const auto result = run(
      "link \"./g.clp\" as G; auto player = G.Id(G.Player { Health = 14 }); post(player.Health); post(G.Id(player).Health);",
      {{"./g.clp", "export def Player { number Health; } export template <typename T> T Id(T value) { return value; }"}});
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "14\n14\n");
  REQUIRE_FALSE(run(
      "link \"./a.clp\" as A; link \"./b.clp\" as B; template <typename T> T Choose(T a, T b) { return a; } Choose(A.Player { Health = 1 }, B.Player { Health = 2 });",
      {{"./a.clp", "export def Player { number Health; }"}, {"./b.clp", "export def Player { number Health; }"}}).compiled);
}

TEST_CASE("methods on generic data preserve receiver type arguments", "[regression][generic-methods]") {
  for (const char* source : {
      "def Box<T> { T Value; T Get() { return @this.Value; } } auto b = Box<number> { Value = 1 }; string text = b.Get();",
      "def Box<T> { T Value; T Bad() { return 1; } }",
      "def Box<T> { T Value; T Replace(T value) { return value; } } auto b = Box<number> { Value = 1 }; b.Replace(\"bad\");",
      "def Box<T> { array<T> Items; array<T> Get() { return @this.Items; } } auto b = Box<number> { Items = array<number>(1) }; array<string> values = b.Get();",
      "def Box<T> { T Value; void Bad(string text) { @this.Value = text; } }",
      "def Box<T> { T Value; func Other<U>(U value) -> U { return value; } }"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Player { number Health; }
def Box<T> {
  T Value;
  T Get() { return @this.Value; }
  T Replace(T value) { return value; }
  Box<T> Same() { return @this; }
  array<T> Items() { return array<T>(@this.Get()); }
}
template <typename T> Box<T> MakeBox(T value) { return Box<T> { Value = value }; }
auto box = MakeBox(16); post(box.Get()); post(box.Replace(17));
auto player = MakeBox(Player { Health = 18 });
auto result = player.Get(); post(result.Health);
post(player.Same().Get().Health);
post(MakeBox(Player { Health = 19 }).Get().Health);
post(player.Items()[0].Health);
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "16\n17\n18\n18\n19\n18\n");
  const auto imported = run(
      "link \"./box.clp\" as B; auto box = B.Box<number> { Value = 20 }; post(box.Get());",
      {{"./box.clp", "export def Box<T> { T Value; T Get() { return @this.Value; } }"}});
  INFO(imported.error); REQUIRE(imported.compiled); REQUIRE(imported.ran);
  REQUIRE(imported.output == "20\n");
}

TEST_CASE("generic nominal bounds expose checked fields and methods", "[regression][generic-bounds]") {
  for (const char* source : {
      "def Scorable { number Points; } template <typename T : Scorable> number Read(T value) { return value.Missing; }",
      "def Scorable { number Points; } template <typename T : Scorable> string Read(T value) { return value.Points; }",
      "def Scorable { number Points; } template <typename T : Scorable> void Set(T value) { value.Points = \"bad\"; }",
      "def Scorable { number Points; } template <typename T : Scorable> number Read(T value) { return value.Points; } Read(1);",
      "def Scorable { number Points; } def Similar { number Points; } template <typename T : Scorable> number Read(T value) { return value.Points; } Read(Similar { Points = 1 });",
      "def Scorable { number Points; } template <typename T : Scorable> T Bad(Scorable value) { return value; }",
      "def Scorable { number Score() { return 1; } } def Bad : Scorable { string Score() { return \"bad\"; } } template <typename T : Scorable> number Read(T value) { return value.Score(); } Read(Bad {});",
      "def Scorable { number Score(number value) { return value; } } def Bad : Scorable { number Score(string value) { return 1; } } template <typename T : Scorable> number Read(T value) { return value.Score(1); } Read(Bad {});",
      "def Scorable { number Points; } template <typename T : Scorable> number Read(T value) { return value.Points; } void Test(any value) { Read<any>(value); }",
      "def Scorable { number Score() { return 1; } } def Bad : Scorable { async number Score() { return 2; } } template <typename T : Scorable> number Read(T value) { return value.Score(); } Read(Bad {});",
      "def A { number First; } def B { number Points; } def Both : A, B {} template <typename T : B> number Read(T value) { return value.Points; } Read(Both { First = 1, Points = 2 });"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Scorable { number Points; number Score(number extra) { return @this.Points + extra; } }
def Player : Scorable { string Name; number Score(number extra) { return @this.Points + extra + 10; } }
def Stats { number Health; }
def WithStats { Stats Data; }
def Box<T> { T Value; T Get() { return @this.Value; } }
template <typename T : Scorable> number Read(T value) { return value.Points; }
template <typename T : Scorable> number ViaMethod(T value) { return value.Score(1); }
number BaseRead(Scorable value) { return value.Points; }
template <typename T : Scorable> number Delegate(T value) { return BaseRead(value); }
template <typename T : Scorable> number Sum(array<T> values) {
  number total = 0;
  for (T value in values) { total += value.Points; }
  return total;
}
template <typename T : WithStats> number Health(T value) { auto data = value.Data; return data.Health; }
template <typename T : Box<Stats>> number BoxHealth(T value) { return value.Value.Health; }
template <typename T : Box<Stats>> number MethodHealth(T value) { return value.Get().Health; }
auto player = Player { Points = 3, Name = "Ada" };
post(Read(player)); post(Read<Player>(player)); post(ViaMethod(player)); post(Delegate(player));
post(Sum(array<Player>(player, Player { Points = 4, Name = "Rui" })));
post(Health(WithStats { Data = Stats { Health = 21 } }));
post(BoxHealth(Box<Stats> { Value = Stats { Health = 22 } }));
post(MethodHealth(Box<Stats> { Value = Stats { Health = 23 } }));
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "3\n3\n14\n3\n7\n21\n22\n23\n");
}

TEST_CASE("nominal bounds respect formal scopes and imported identities", "[regression][generic-bounds][modules]") {
  const auto result = run(
      "link \"./scores.clp\" as Scores; def T { string Wrong; } template <typename T : Scores.Scorable> number Read(T value) { return value.Points; } post(Read(Scores.Scorable { Points = 24 }));",
      {{"./scores.clp", "export def Scorable { number Points; }"}});
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran); REQUIRE(result.output == "24\n");
  REQUIRE_FALSE(run(
      "link \"./a.clp\" as A; link \"./b.clp\" as B; template <typename T : A.Scorable> number Read(T value) { return value.Points; } Read(B.Scorable { Points = 1 });",
      {{"./a.clp", "export def Scorable { number Points; }"}, {"./b.clp", "export def Scorable { number Points; }"}}).compiled);
}

TEST_CASE("methods declare their own generic parameters separately from the receiver", "[regression][method-templates]") {
  for (const char* source : {
      "def Box<T> { T Value; template <typename T> T Echo(T value) { return value; } }",
      "def Box<T> { T Value; template <typename U> U Echo(U value) { return value; } } auto box = Box<number> { Value = 1 }; box.Echo<number>(\"bad\");",
      "def Box<T> { T Value; template <typename U> U Echo(U value) { return value; } } auto box = Box<number> { Value = 1 }; box.Echo<number, string>(1);",
      "def Tool { template <typename U> U Echo(U value) { return value; } } auto tool = Tool {}; string text = tool.Echo(1);",
      "def Tool { template <typename U> U Bad(U value) { return 1; } }",
      "def Tool { template <typename U> optional<U> Empty() { return null; } } auto tool = Tool {}; tool.Empty();",
      "def Tool { template <typename U> U Echo(U value) { return value; } } auto tool = Tool {}; tool.Echo<Missing>(1);",
      "def Tool { template <typename U> async U Echo(U value) { return value; } } auto tool = Tool {}; string text = await tool.Echo(1);",
      "def Base { template <typename U> U Echo(U value) { return value; } } def Bad : Base { number Echo(number value) { return value; } }",
      "def U { number Value; } def Tool { template <typename U> void Bad() { auto value = U { Value = 1 }; } }",
      "def Box<T> { T Value; T Keep(T value) { return value; } } auto box = Box<number> { Value = 1 }; box.Keep<number>(1);",
      "def Base { template <typename U> U Echo(U first, U second) { return first; } } def Bad : Base { template <typename V> V Echo(V first, number second) { return first; } }",
      "def Base { template <typename U> U Echo(U value) { return value; } } def Bad : Base { template <typename V : number> V Echo(V value) { return value; } }"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run("\n"
  "def Player { number Health; }\n"
  "def Box<T> {\n"
  "  T Value;\n"
  "  T Keep(T value) { return value; }\n"
  "  template <typename U> U Echo(U value) { return value; }\n"
  "  template <typename U> Box<U> Wrap(U value) { return Box<U> { Value = value }; }\n"
  "  template <typename U> U Nested(U value) { return @this.Echo<U>(value); }\n"
  "  template <typename U : Player> number Health(U value) { return value.Health; }\n"
  "  template <typename U> optional<U> Empty() { return null; }\n"
  "  template <typename U : T> U Related(U value) { return value; }\n"
  "}\n"
  "Box<number> Make() { return Box<number> { Value = 1 }; }\n"
  "auto box = Make();\n"
  "post(box.Echo(\"ok\")); post(box.Echo<string>(\"typed\"));\n"
  "auto player = box.Echo(Player { Health = 25 }); post(player.Health);\n"
  "post(Make().Echo<Player>(player).Health);\n"
  "post(box.Wrap(player).Value.Health);\n"
  "post(box.Wrap<Player>(player).Echo<number>(26));\n"
  "post(box.Nested(player).Health); post(box.Health(player));\n"
  "optional<string> empty = box.Empty<string>(); post(empty == null);\n"
  "auto floats = Box<float> { Value = 1.5 }; post(floats.Keep(2));\n"
  "post(floats.Related(3));\n"
  "def Base { template <typename U> U Echo(U value) { return value; } }\n"
  "def Child : Base { template <typename V> V Echo(V value) { return value; } }\n"
  "Base polymorphic = Child {}; post(polymorphic.Echo<string>(\"virtual\"));\n");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "ok\ntyped\n25\n25\n25\n26\n25\n25\ntrue\n2\n3\nvirtual\n");
}

TEST_CASE("imported method templates preserve owner and method specializations", "[regression][method-templates][modules]") {
  const auto result = run(
      "link \"./box.clp\" as B; auto box = B.Box<number> { Value = 1 }; auto player = box.Echo<B.Player>(B.Player { Health = 27 }); post(player.Health); post(box.Echo(player).Health);",
      {{"./box.clp", "export def Player { number Health; } export def Box<T> { T Value; template <typename U> U Echo(U value) { return value; } }"}});
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "27\n27\n");
}

TEST_CASE("generic inheritance substitutes base arguments fields and inherited methods", "[regression][generic-inheritance]") {
  for (const char* source : {
      "def Base<T> { T Value; } def Child<U> : Base {}",
      "def Base<T> { T Value; } def Child<U> : Base<U, number> {}",
      "def Base<T> { T Value; } def Child<U> : Base<Missing> {}",
      "def Base<T> { T Value; } def Child<U> : Base<U> {} auto value = Child<number> { Value = \"bad\" };",
      "def Base<T> { T Value; } def Child<U> : Base<U> {} Base<string> value = Child<number> { Value = 1 };",
      "def Base<T> { T Value; } def Child<U> : Base<U> {} Base<float> value = Child<number> { Value = 1 };",
      "def Base<T> { T Value; T Get() { return @this.Value; } } def Child : Base<number> { string Get() { return \"bad\"; } }",
      "def Base<T> { T Value; } def Other {} def Child<U> : Base<U>, Other {}",
      "def A : B {} def B : A {}",
      "def A<T> : A<T> {}"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Base<T> {
  T Value;
  T Get() { return @this.Value; }
  template <typename V> V Echo(V value) { return value; }
}
def Child<U> : Base<U> { string Name; }
def Nested<V> : Base<array<V>> { }
def Grand<W> : Child<W> { }
def Fixed : Base<number> { number Get() { return @this.Value + 1; } }
template <typename T> T Read(Base<T> value) { return value.Value; }
auto child = Child<number> { Value = 30, Name = "Ada" };
Base<number> base = child; post(base.Value); post(child.Get()); post(Read(child));
post(child.Echo<string>("ok"));
auto nested = Nested<number> { Value = array<number>(31, 32) };
post(nested.Value[0]); post(nested.Get()[1]); post(Read(nested)[0]);
auto grand = Grand<number> { Value = 33, Name = "Rui" };
post(grand.Get()); Base<number> root = grand; post(root.Value);
auto fixed = Fixed { Value = 34 }; Base<number> dynamic = fixed; post(dynamic.Get());
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "30\n30\n30\nok\n31\n32\n31\n33\n33\n35\n");
}

TEST_CASE("generic bases preserve module identity aliases and renamed override parameters", "[regression][generic-inheritance][modules]") {
  const auto imported = run(
      "link \"./base.clp\" as B; def U { string Wrong; } def Child<U> : B.Base<array<U>> {} auto value = Child<number> { Value = array<number>(36) }; post(value.Get()[0]);",
      {{"./base.clp", "export def Base<T> { T Value; T Get() { return @this.Value; } }"}});
  INFO(imported.error); REQUIRE(imported.compiled); REQUIRE(imported.ran);
  REQUIRE(imported.output == "36\n");
  const auto aliases = run(R"CLPP(
def Base<T> { T Value; template <typename V> V Echo(V value) { return value; } }
def IntegerBase = Base<number>;
def Fixed : IntegerBase { template <typename W> W Echo(W value) { return value; } }
Base<number> value = Fixed { Value = 37 }; post(value.Value); post(value.Echo<string>("virtual"));
def Defaults<T> { optional<T> Value = null; }
def Child<U> : Defaults<U> {}
post(Child<number> {}.Value == null);
)CLPP");
  INFO(aliases.error); REQUIRE(aliases.compiled); REQUIRE(aliases.ran);
  REQUIRE(aliases.output == "37\nvirtual\ntrue\n");
  REQUIRE_FALSE(run("def Base<T> { template <typename V> V Echo(V value) { return value; } } def Fixed : Base<number> { template <typename W> string Echo(W value) { return \"bad\"; } }").compiled);
}

TEST_CASE("ordinary overrides and base views preserve VM slot contracts", "[regression][subtype-contracts]") {
  for (const char* source : {
      "def Base { number Get() { return 1; } } def Bad : Base { string Get() { return \"bad\"; } }",
      "def Base { number Get(number value) { return value; } } def Bad : Base { number Get(string value) { return 1; } }",
      "def Base { number Get(float value) { return 1; } } def Bad : Base { number Get(number value) { return value; } }",
      "def Base { number Get(number value) { return value; } } def Bad : Base { number Get() { return 1; } }",
      "def Left { number First; } def Right { number Second; } def Both : Left, Right {} Right value = Both { First = 1, Second = 2 };",
      "def Left { number One() { return 1; } } def Right { number Two() { return 2; } } def Both : Left, Right {} Right value = Both {}; post(value.Two());"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
def Base { number Value; number Get(number extra) { return @this.Value + extra; } }
def Child : Base { number Get(float extra) { return @this.Value + 2; } }
Base value = Child { Value = 39 }; post(value.Get(1));
def Result { number Value; }
def Detailed : Result { string Name; }
def Factory { Result Make() { return Result { Value = 40 }; } }
def Specific : Factory { Detailed Make() { return Detailed { Value = 41, Name = "Ada" }; } }
Factory factory = Specific {}; post(factory.Make().Value);
def Left { number First; }
abstract def Right { abstract number GetSecond(); }
def Both : Left, Right { number Second; number GetSecond() { return @this.Second; } }
Left first = Both { First = 42, Second = 43 }; post(first.First);
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "41\n41\n42\n");
}

TEST_CASE("VM integers preserve all 64 bits and report arithmetic overflow", "[regression][integer-storage]") {
  const auto exact = run(R"CLPP(
post(9007199254740993); post(9007199254740993 + 2);
post(9223372036854775807); post(-9223372036854775808);
post(9007199254740993 > 9007199254740992);
post(9223372036854775807 / 3);
post(-9223372036854775808 % -1);
post(0x7fff_ffff_ffff_ffff);
def Box { number Value; } auto box = Box { Value = 9007199254740993 }; post(box.Value);
number Identity(number value) { return value; } post(Identity(9007199254740993));
)CLPP");
  INFO(exact.error); REQUIRE(exact.compiled); REQUIRE(exact.ran);
  REQUIRE(exact.output == "9007199254740993\n9007199254740995\n9223372036854775807\n-9223372036854775808\ntrue\n3074457345618258602\n0\n9223372036854775807\n9007199254740993\n9007199254740993\n");
  for (const char* source : {"post(9223372036854775807 + 1);", "post(-9223372036854775808 - 1);", "post(9223372036854775807 * 2);", "post(-9223372036854775808 / -1);", "auto value = -9223372036854775808; post(-value);"}) {
    const auto result = run(source); INFO(source); REQUIRE(result.compiled); REQUIRE_FALSE(result.ran); REQUIRE(result.error == "integer overflow");
  }
  REQUIRE_FALSE(run("post(9223372036854775808);").compiled);
  REQUIRE_FALSE(run("post(-9223372036854775809);").compiled);
  const auto imported = run("link \"./numbers.clp\" as N; post(N.BIG);", {{"./numbers.clp", "export const number BIG = 9007199254740993;"}});
  INFO(imported.error); REQUIRE(imported.compiled); REQUIRE(imported.ran); REQUIRE(imported.output == "9007199254740993\n");
}

TEST_CASE("mixed comparisons bit operations and constant folding retain integer precision", "[regression][integer-storage]") {
  const auto result = run(R"CLPP(
post(9007199254740993 > 9007199254740992.0);
post(9007199254740993 == 9007199254740992.0);
post(9223372036854775807 < 9223372036854775808.0);
post(9007199254740993 & 0x7fff_ffff_ffff_ffff);
post(~9223372036854775807);
constexpr number BIG = 9007199254740993 + 2; post(BIG);
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "true\nfalse\ntrue\n9007199254740993\n-9223372036854775808\n9007199254740995\n");
  REQUIRE_FALSE(run("constexpr number BIG = 9223372036854775807 + 1;").compiled);
}

TEST_CASE("public numeric declarations use only number and float", "[regression][numeric-names]") {
  const auto result = run("number count = 44; float rate = 1.5; const auto limit = 45; post(count); post(rate); post(limit);");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran); REQUIRE(result.output == "44\n1.5\n45\n");
  REQUIRE_FALSE(run("int count = 1;").compiled);
  REQUIRE_FALSE(run("double rate = 1.5;").compiled);
}

TEST_CASE("numeric casts convert explicitly and reject out of range values", "[regression][numeric-casts]") {
  for (const char* source : {"number value = 1.5;", "post(static_cast<number>(\"1\"));", "post(static_cast<bool>(1));", "post(static_cast<Missing>(1));", "post(static_cast<number>(null));"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto valid = run(R"CLPP(
post(static_cast<number>(3.9)); post(static_cast<number>(-3.9));
post(static_cast<float>(7) / 2); post(static_cast<number>(9007199254740993));
def Count = number; post(static_cast<Count>(5.5));
post(static_cast<number>(-9223372036854775808.0));
)CLPP");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  REQUIRE(valid.output == "3\n-3\n3.5\n9007199254740993\n5\n-9223372036854775808\n");
  for (const char* source : {"post(static_cast<number>(9223372036854775808.0));", "post(static_cast<number>(-9223372036854777856.0));", "post(static_cast<number>(1e300 * 1e300));"}) {
    const auto result = run(source); INFO(source); REQUIRE(result.compiled); REQUIRE_FALSE(result.ran); REQUIRE(result.error == "numeric cast out of range");
  }
  const auto imported = run("link \"./types.clp\" as T; post(static_cast<T.Count>(6.9));", {{"./types.clp", "export def Count = number;"}});
  INFO(imported.error); REQUIRE(imported.compiled); REQUIRE(imported.ran); REQUIRE(imported.output == "6\n");
}

TEST_CASE("integer increments and loops do not reintroduce floating precision loss", "[regression][integer-storage]") {
  const auto result = run(R"CLPP(
number value = 9007199254740993;
post(value++); post(value); --value; post(value);
for (number index = 9007199254740993; index < 9007199254740996; index++) { post(index); }
float decimal = 1.5; decimal++; post(decimal);
try { number edge = 9223372036854775807; edge++; } catch (error) { post(error); }
try { post(static_cast<number>(9223372036854775808.0)); } catch (error) { post(error); }
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "9007199254740993\n9007199254740994\n9007199254740993\n9007199254740993\n9007199254740994\n9007199254740995\n2.5\ninteger overflow\nnumeric cast out of range\n");
}

TEST_CASE("atomic integers remain exact and overflow leaves the cell unchanged", "[regression][integer-storage]") {
  const auto result = run(R"CLPP(
atomic value = 9007199254740993;
post(fetch_add(value, 2)); post(atomic_load(value));
atomic edge = 9223372036854775807;
try { fetch_add(edge, 1); } catch (error) { post(error); }
post(atomic_load(edge));
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "9007199254740993\n9007199254740995\ninteger overflow\n9223372036854775807\n");
}

TEST_CASE("implicit number to float conversion happens at checked value boundaries", "[regression][numeric-casts]") {
  const auto result = run(R"CLPP(
float value = 9007199254740993; post(value == 9007199254740992.0);
value = 9007199254740993; post(value == 9007199254740992.0);
float Promote(number value) { return value; }
float Echo(float value) { return value; }
post(Promote(9007199254740993) == 9007199254740992.0);
post(Echo(9007199254740993) == 9007199254740992.0);
def Box<T> { T Value; }
auto box = Box<float> { Value = 9007199254740993 };
post(box.Value == 9007199254740992.0);
box.Value = 9007199254740993; post(box.Value == 9007199254740992.0);
auto values = array<float>(9007199254740993); post(values[0] == 9007199254740992.0);
optional<float> maybe = 9007199254740993; post(maybe == 9007199254740992.0);
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "true\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\n");
}

TEST_CASE("postfix updates preserve earlier values in expression evaluation", "[regression][integer-storage]") {
  const auto result = run("number value = 2; post(value + value++); post(value); post(++value + value++); post(value);");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "4\n3\n8\n5\n");
}

TEST_CASE("virtual numeric contracts normalize broader inputs and covariant results", "[regression][numeric-casts]") {
  const auto result = run(R"CLPP(
def Base { bool Read(number value) { return false; } float Make() { return 0.0; } }
def Child : Base {
  bool Read(optional<float> value) { guard (value != null) else { return false; } return value == 9007199254740992.0; }
  number Make() { return 9007199254740993; }
}
Base view = Child {}; post(view.Read(9007199254740993)); post(view.Make() == 9007199254740992.0);
optional<float> Convert(optional<number> value) { return value; }
post(Convert(9007199254740993) == 9007199254740992.0); post(Convert(null) == null);
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "true\ntrue\ntrue\ntrue\n");
}

TEST_CASE("numeric generic bounds prove arithmetic results without dynamic guesses", "[regression][numeric-bounds]") {
  for (const char* source : {
      "template <typename T> T Bad(T value) { return value + 1; }",
      "template <typename T : number | string> T Bad(T value) { return value + 1; }",
      "template <typename T : number> T Bad(T value) { return value + 1.5; }",
      "template <typename T : float> T Bad(T value) { return value & 1; }",
      "post(1.5 & 1);", "post(~true);", "float value = 1.5; number result = value++;",
      "template <typename T : number> T Twice(T value) { return value * 2; } Twice(1.5);"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto result = run(R"CLPP(
template <typename T : number | float> T Half(T value) { return value / 2; }
template <typename T : number | float> T Add(T value, T other) { return value + other; }
template <typename T : float> T Twice(T value) { return value * 2; }
template <typename T : number> T Bit(T value) { return value & 7; }
template <typename T : number | float> bool Less(T value, T other) { return value < other; }
template <typename T : number | float> float Decimal(T value) { return static_cast<float>(value); }
template <typename T : number & float> T Increment(T value) { value++; return value; }
number whole = Half(7); float fraction = Half(7.0); post(whole); post(fraction);
post(Add(9007199254740993, 2)); post(Add(1.5, 2.5));
post(Twice(3)); post(Twice(3.5)); post(Bit(15)); post(Less(2, 3));
post(Decimal(5)); post(Increment(6));
def Score = number; Score first = 1; Score second = 2; post(first + second);
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "3\n3.5\n9007199254740995\n4\n6\n7\n7\ntrue\n5\n7\n3\n");
}

TEST_CASE("typed collections preserve exact values through sorting slices and mutation", "[regression][collection-contracts]") {
  for (const char* source : {
      "auto values = array<number>(1); push(values, \"bad\");",
      "auto values = array<number>(1); insert(values, 0.5, 2);",
      "auto values = array<number>(1); remove(values, \"bad\");",
      "auto values = array<string>(\"a\"); number value = pop(values);",
      "auto values = array<number>(1); number value = push(values, 2);",
      "auto values = dictionary<string, number>(\"a\", 1); post(values[1]);",
      "auto values = array<number>(1); post(values[0.0]);",
      "post(sort(array<string>(\"b\", \"a\")));",
      "def Data { number Value; } post(len(Data { Value = 1 }));",
      "auto values = array<number>(1); find(values, \"bad\");",
      "auto values = dictionary<string, number>(\"a\", 1); post(values[0..1]);"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto result = run(R"CLPP(
auto values = sort(array<number>(9007199254740995, 9007199254740993, 9007199254740994));
post(values); post(find(values, 9007199254740994));
push(values, 9007199254740996); post(pop(values));
insert(values, 0, 9007199254740992); post(remove(values, 0));
def Player { number Health; }
auto team = array<Player>(Player { Health = 46 }, Player { Health = 47 });
auto subset = team[0..1]; post(subset[0].Health);
post("é🙂x"[0..2]); post("é🙂x"[1..2]);
post(len(0..5000)); post(9007199254740993..9007199254740996);
post(values[0..9223372036854775807]);
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "[9007199254740993, 9007199254740994, 9007199254740995]\n1\n9007199254740996\n9007199254740992\n46\né🙂\n🙂\n5000\n[9007199254740993, 9007199254740994, 9007199254740995]\n[9007199254740993, 9007199254740994, 9007199254740995]\n");
}

TEST_CASE("dictionary lookup uses typed keys and missing keys raise errors", "[regression][collection-contracts]") {
  const auto result = run(R"CLPP(
auto names = dictionary<number, string>(2, "two", 9007199254740993, "big");
post(names[2]); post(names[9007199254740993]);
names[2] = "updated"; post(names[2]); names[3] = "three";
post(remove(names, 3)); post(len(names));
try { post(names[1]); } catch (error) { post("missing"); }
try { remove(names, 1); } catch (error) { post("missing remove"); }
post(len(names));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "two\nbig\nupdated\nthree\n2\nmissing\nmissing remove\n2\n");
}

TEST_CASE("invalid large indices and NaN sorting fail safely and can be caught", "[regression][collection-contracts]") {
  const auto result = run(R"CLPP(
auto values = array<number>(1, 2);
try { post(values[9223372036854775807]); } catch (error) { post("read"); }
try { values[-9223372036854775808] = 3; } catch (error) { post("write"); }
try { insert(values, 9223372036854775807, 3); } catch (error) { post("insert"); }
try { remove(values, 9223372036854775807); } catch (error) { post("remove"); }
try { post("x"[9223372036854775807]); } catch (error) { post("text"); }
float nan = (1e300 * 1e300) - (1e300 * 1e300);
try { sort(array<float>(nan)); } catch (error) { post("nan"); }
try { post(-9223372036854775808..9223372036854775807); } catch (error) { post("range"); }
post(values);
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "read\nwrite\ninsert\nremove\ntext\nnan\nrange\n[1, 2]\n");
}

TEST_CASE("for-in validates sources and normalizes declared decimal elements", "[regression][collection-contracts]") {
  for (const char* source : {"for (auto value in 2.5) { post(value); }", "for (auto value in true) { post(value); }", "def Data { number Value; } for (auto value in Data { Value = 1 }) { post(value); }"}) {
    INFO(source); REQUIRE_FALSE(run(source).compiled);
  }
  const auto result = run(R"CLPP(
for (float value in array<number>(3, 5)) { post(value / 2); }
for (float value in 3) { post(value / 2); }
for (optional<float> value in array<number>(3)) { if (value != null) { post(value / 2); } }
template <typename T : number> number Count(T count) { number total = 0; for (auto value in count) { total += value; } return total; }
post(Count(4));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "1.5\n2.5\n0\n0.5\n1\n1.5\n6\n");
}

TEST_CASE("inferred iteration resolves properties of factory and generic elements", "[regression][iteration-types]") {
  const auto result = run(R"CLPP(
def Player { number Health; }
array<Player> MakeTeam() { return array<Player>(Player { Health = 42 }); }
for (auto player in MakeTeam()) { post(player.Health); }
def Roster = array<Player>;
Roster team = MakeTeam(); for (auto player in team) { post(player.Health); }
template <typename T : Player> number HealthTotal(array<T> team) {
  number total = 0; for (auto player in team) { total += player.Health; } return total;
}
post(HealthTotal(MakeTeam()));
for (auto group in array<array<Player>>(MakeTeam())) { for (auto player in group) { post(player.Health); } }
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "42\n42\n42\n42\n");
  REQUIRE_FALSE(run("def Player { number Health; } array<Player> MakeTeam() { return array<Player>(Player { Health = 42 }); } for (auto player in MakeTeam()) { post(player.Missing); }").compiled);
}

TEST_CASE("independent executions isolate synchronization handles", "[regression][runtime-context]") {
  const auto result = clpp::Compiler{}.compile("atomic value = 41; post(atomic_load(value)); post(value); post(mutex());");
  REQUIRE(result.ok());
  std::ostringstream first_output, second_output;
  clpp::VirtualMachine first, second;
  first.set_output(first_output); first.load(result.chunk);
  second.set_output(second_output); second.load(result.chunk);
  REQUIRE(first.run()); REQUIRE(second.run()); REQUIRE(first.run());
  REQUIRE(first_output.str() == "41\n0\n0\n41\n0\n0\n");
  REQUIRE(second_output.str() == "41\n0\n0\n");
  const auto invalid = clpp::Compiler{}.compile("post(atomic_load(0));");
  REQUIRE(invalid.ok()); second.load(invalid.chunk); REQUIRE_FALSE(second.run());
}

TEST_CASE("workers and awaited calls retain the execution synchronization context", "[regression][runtime-context]") {
  const auto result = run(R"CLPP(
number Increment(number cell, number gate) { lock(gate); fetch_add(cell, 1); unlock(gate); return atomic_load(cell); }
async number Later(number cell, number gate) { return Increment(cell, gate); }
atomic value = 0; auto gate = mutex();
auto first = thread Increment(value, gate); auto second = thread Increment(value, gate);
join(first); join(second); post(atomic_load(value));
post(await Later(value, gate));
number Nested(number cell, number gate) { auto child = thread Increment(cell, gate); return join(child); }
auto parent = thread Nested(value, gate); post(join(parent));
number ActorIncrement(number cell) { fetch_add(cell, 1); return atomic_load(cell); }
post(actor(ActorIncrement, value));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "2\n3\n4\n5\n");
}

TEST_CASE("native workers inherit streams and emit complete output records", "[regression][runtime-context]") {
  const auto compiled = clpp::Compiler{}.compile(R"CLPP(
number Emit(number count) { for (auto index in count) { post("line"); warn("notice"); } return count; }
auto first = thread Emit(100); auto second = thread Emit(100);
join(first); join(second);
)CLPP");
  REQUIRE(compiled.ok());
  std::ostringstream output, warnings;
  clpp::VirtualMachine vm; vm.load(compiled.chunk); vm.set_output(output); vm.set_error_output(warnings);
  REQUIRE(vm.run());
  std::string expected_output, expected_warnings;
  for (int index = 0; index < 200; ++index) { expected_output += "line\n"; expected_warnings += "warning: notice\n"; }
  REQUIRE(output.str() == expected_output); REQUIRE(warnings.str() == expected_warnings);
}

TEST_CASE("actor results replace their stack value and preserve the argument binding", "[regression][runtime-context]") {
  const auto result = run("number Twice(number value) { return value * 2; } number value = 21; post(actor(Twice, value)); post(value); post(value + actor(Twice, value));");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "42\n21\n63\n");
}

TEST_CASE("actor descendants preserve host isolation across execution mechanisms", "[regression][runtime-context]") {
  for (const char* operation : {
      "auto child = thread Probe(value); return join(child);",
      "return await ProbeAsync(value);",
      "auto values = parallel(ProbeAsync(value), ProbeAsync(value)); return values[0];"}) {
    const auto result = run(std::string("number Probe(number value) { return fs::size(\"missing\"); } async number ProbeAsync(number value) { return Probe(value); } number Outer(number value) { ") + operation + " } post(actor(Outer, 1));");
    INFO(operation); INFO(result.error); INFO(result.detail);
    REQUIRE(result.compiled); REQUIRE_FALSE(result.ran); REQUIRE(result.error == "sandbox");
  }
}

TEST_CASE("embedding teardown drains workers and descendants before releasing streams", "[regression][runtime-context]") {
  const auto compiled = clpp::Compiler{}.compile(R"CLPP(
number Child(number value) { for (auto index in 10000) {} post("child done"); return value; }
number Parent(number value) { auto child = thread Child(value); return value; }
auto parent = thread Parent(1);
)CLPP");
  REQUIRE(compiled.ok());
  std::ostringstream output;
  {
    clpp::VirtualMachine vm; vm.load(compiled.chunk); vm.set_output(output);
    REQUIRE(vm.run());
  }
  REQUIRE(output.str() == "child done\n");
  const auto reused = clpp::Compiler{}.compile("number Worker(number value) { post(\"done\"); return value; } auto worker = thread Worker(1);");
  REQUIRE(reused.ok());
  output.str("");
  {
    clpp::VirtualMachine vm; vm.load(reused.chunk); vm.set_output(output);
    REQUIRE(vm.run()); REQUIRE(vm.run());
  }
  REQUIRE(output.str() == "done\ndone\n");
}

TEST_CASE("program completion waits for unjoined worker descendants", "[regression][thread-completion]") {
  const auto compiled = clpp::Compiler{}.compile(R"CLPP(
number Child(number value) { for (auto index in 10000) {} post("completed"); return value; }
number Parent(number value) { auto child = thread Child(value); return value; }
auto parent = thread Parent(1);
)CLPP");
  REQUIRE(compiled.ok()); std::ostringstream output;
  clpp::VirtualMachine vm; vm.load(compiled.chunk); vm.set_output(output);
  REQUIRE(vm.run()); REQUIRE(output.str() == "completed\n");
}

TEST_CASE("unobserved worker failures fail program completion with their source location", "[regression][thread-completion]") {
  const auto compiled = clpp::Compiler{}.compile("number Fail(number value) {\n  number divisor = 0;\n  return value / divisor;\n}\nauto worker = thread Fail(1);\n");
  REQUIRE(compiled.ok()); clpp::VirtualMachine vm; vm.load(compiled.chunk);
  REQUIRE_FALSE(vm.run()); REQUIRE(vm.error() == "division by zero"); REQUIRE(vm.error_location().line == 3);
}

TEST_CASE("caught worker failures are observed and do not fail completion again", "[regression][thread-completion]") {
  const auto result = run(R"CLPP(
number Fail(number value) { report("worker failure"); return value; }
number CatchChild(number value) {
  auto child = thread Fail(value);
  try { join(child); } catch (error) { post("parent caught"); }
  return value;
}
auto first = thread Fail(1); try { join(first); } catch (error) { post("main caught"); }
auto parent = thread CatchChild(1);
number CatchExisting(number handle) { try { join(handle); } catch (error) {} return 0; }
auto earlier = thread Fail(1); auto later = thread CatchExisting(earlier);
)CLPP");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output == "main caught\nparent caught\n");
}

TEST_CASE("main failures still wait for workers and retain the primary error", "[regression][thread-completion]") {
  const auto compiled = clpp::Compiler{}.compile(R"CLPP(
number Work(number value) { for (auto index in 10000) {} post("worker finished"); report("secondary"); return value; }
auto worker = thread Work(1);
report("primary");
)CLPP");
  REQUIRE(compiled.ok()); std::ostringstream output; clpp::VirtualMachine vm;
  vm.load(compiled.chunk); vm.set_output(output);
  REQUIRE_FALSE(vm.run()); REQUIRE(vm.error() == "primary"); REQUIRE(output.str() == "worker finished\n");
}

TEST_CASE("awaited failures preserve their source through workers and consumed joins", "[regression][thread-completion]") {
  const auto compiled = clpp::Compiler{}.compile("async number Fail(number value) {\n  number divisor = 0;\n  return value / divisor;\n}\nnumber Worker(number value) { return await Fail(value); }\nauto work = thread Worker(1);\njoin(work);\n");
  REQUIRE(compiled.ok()); clpp::VirtualMachine vm; vm.load(compiled.chunk);
  REQUIRE_FALSE(vm.run()); REQUIRE(vm.error() == "division by zero"); REQUIRE(vm.error_location().line == 3);
}

TEST_CASE("data arrays and dictionaries share identity through aliases parameters and methods", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
def Player { number Health; void Damage(number amount) { @this.Health -= amount; } }
void Change(Player player, array<number> values, dictionary<string, number> names) {
  player.Health = 80; values[0] = 2; names["score"] = 3;
}
auto player = Player { Health = 100 }; auto alias = player;
auto values = array<number>(1); auto other = values;
auto names = dictionary<string, number>("score",1); auto copy = names;
Change(alias,other,copy); post(player.Health); post(values[0]); post(names["score"]);
alias.Damage(5); post(player.Health);
post(alias == player); post(player == Player { Health = 75 });
post(values == other); post(values == array<number>(2));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="80\n2\n3\n75\ntrue\nfalse\ntrue\nfalse\n");
}

TEST_CASE("const freezes every existing alias and nested aggregate", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
def Player { number Health; void Damage(number value) { @this.Health -= value; } }
def Team { array<Player> Players; }
auto player = Player { Health = 100 }; auto players = array<Player>(player);
auto team = Team { Players = players }; const auto fixed = team;
try { player.Health = 1; } catch (error) { post("field frozen"); }
try { player.Damage(1); } catch (error) { post("method frozen"); }
try { push(players,Player { Health = 1 }); } catch (error) { post("array frozen"); }
auto names = dictionary<string, Player>("hero",player); const auto frozen = names;
try { names["other"] = Player { Health = 1 }; } catch (error) { post("dictionary frozen"); }
post(team.Players[0].Health); post(len(players));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="field frozen\nmethod frozen\narray frozen\ndictionary frozen\n100\n1\n");
}

TEST_CASE("shared objects survive worker return and synchronized cross thread mutation", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
def Counter { number Value = 0; array<number> Values; }
number Increment(Counter counter, number gate) {
  for (auto index in 1000) { lock(gate); counter.Value += 1; push(counter.Values,index); unlock(gate); } return 1;
}
Counter Make(number value) { return Counter { Value = value, Values = array<number>(value) }; }
auto counter = Counter { Values = array<number>() }; auto gate = mutex();
auto first = thread Increment(counter,gate); auto second = thread Increment(counter,gate);
join(first); join(second); post(counter.Value); post(len(counter.Values));
auto factory = thread Make(42); Counter returned = join(factory); post(returned.Value); post(returned.Values[0]);
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="2000\n2000\n42\n42\n");
}

TEST_CASE("source cycles preserve identity print safely and freeze transitively", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
def Node { number Value = 1; optional<Node> Next = null; }
auto first = Node {}; auto second = Node {}; first.Next = second; second.Next = first;
post(first.Next == second); post(first);
const auto fixed = first;
try { second.Value = 2; } catch (error) { post("cycle frozen"); }
post(first.Value); post(second.Value);
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="true\n(1, (1, <cycle>))\ncycle frozen\n1\n1\n");
}

TEST_CASE("collection mutators operate on shared fields indices and factory expressions", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
def Bag { array<number> Items; dictionary<string,number> Names; }
auto bag = Bag { Items = array<number>(1,2), Names = dictionary<string,number>("x",3) };
auto alias = bag.Items; push(bag.Items,4); insert(bag.Items,1,9);
post(pop(bag.Items)); post(remove(bag.Items,1)); post(alias);
post(remove(bag.Names,"x")); post(len(bag.Names));
auto groups = array<array<number>>(alias); push(groups[0],5); post(alias);
array<number> Make() { return array<number>(7,8); } post(pop(Make()));
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="4\n9\n[1, 2]\n3\n0\n[1, 2, 5]\n8\n");
}

TEST_CASE("native JSON updates create a new parent and serialization rejects cycles", "[regression][shared-heap]") {
  const auto result=run(R"CLPP(
link @clpp.json as Json;
const auto original = Json.Parse("{\"value\":1}");
auto changed = Json.Set(original,"value",2); post(Json.Stringify(original)); post(Json.Stringify(changed));
def Node { optional<Node> Next = null; } auto node = Node {}; node.Next = node;
try { Json.Stringify(node); } catch (error) { post("cycle rejected"); }
)CLPP");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  REQUIRE(result.output=="{\"value\":1}\n{\"value\":2}\ncycle rejected\n");
}


TEST_CASE("module state initializes once and aliases share it", "[modules-state]") {
  const auto result = run(R"(
    link "./state.clp" as A;
    link "./folder/../state.clp" as B;
    post(A.Next()); post(B.Next()); post(A.LIMIT);
  )", {{"./state.clp", R"(
    number count = 10;
    export const number LIMIT = 100;
    post("init");
    number Increment() { count += 1; return count; }
    export number Next() { return Increment(); }
  )"}});
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "init\n11\n12\n100\n");
}

TEST_CASE("module visibility applies to functions types constants and IDE metadata", "[modules-state]") {
  const std::map<std::string, std::string> files = {{"./m.clp", R"(
    def Hidden { number Value; }
    def SecretAlias = number;
    const number SECRET = 3;
    number Private() { return SECRET; }
    export number Public() { return Private(); }
  )"}};
  REQUIRE(run("link \"./m.clp\" as M; post(M.Public());", files).output == "3\n");
  for (const auto& source : {"M.Private();", "post(M.SECRET);", "M.Hidden h = M.Hidden { Value = 1 };", "M.SecretAlias value = 1;"}) {
    INFO(source); CHECK_FALSE(run(std::string("link \"./m.clp\" as M; ") + source, files).compiled);
  }
  const auto analysis = clpp::analyze_program("link \"./m.clp\" as M;", [&](std::string_view) -> std::optional<std::string> { return files.begin()->second; });
  REQUIRE(analysis.ok()); REQUIRE(analysis.modules.size() == 1);
  CHECK(analysis.modules[0].functions == std::vector<std::string>{"Public"});
  CHECK(analysis.modules[0].types.empty()); CHECK(analysis.modules[0].constants.empty());
}

TEST_CASE("module dependencies initialize before dependents and state survives closures", "[modules-state]") {
  const auto result = run("link \"./a.clp\" as A; link \"./b.clp\" as B; post(A.Next()); post(B.Next());", {
    {"./a.clp", R"(
      link "./state.clp" as S;
      export number Next() { return S.Next(); }
      post("a");
    )"},
    {"./b.clp", R"(
      link "./state.clp" as S;
      export number Next() { return S.Next(); }
      post("b");
    )"},
    {"./state.clp", R"(
      export def Read = () -> number;
      number count = 0;
      Read read = func() -> number { count++; return count; };
      export number Next() { return read(); }
      post("state");
    )"}
  });
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "state\na\nb\n1\n2\n");
}


TEST_CASE("module state is fresh for every program run", "[modules-state]") {
  const auto compiled = clpp::Compiler{}.compile("link \"./m.clp\" as M; post(M.Next());",
    [](std::string_view) -> std::optional<std::string> { return "number count = 1; post(\"init\"); export number Next() { count++; return count; }"; });
  REQUIRE(compiled.ok()); clpp::VirtualMachine vm; std::ostringstream out; vm.set_output(out); vm.load(compiled.chunk);
  REQUIRE(vm.run()); REQUIRE(vm.run()); CHECK(out.str() == "init\n2\ninit\n2\n");
}

TEST_CASE("module state shares synchronized worker mutations", "[modules-state]") {
  const auto result = run(R"(
    link "./m.clp" as M;
    number a = thread M.Add(20); number b = thread M.Add(22);
    join(a); join(b); post(M.Read());
  )", {{"./m.clp", R"(
    number value = 0; number gate = mutex();
    export number Add(number amount) { lock(gate); value += amount; unlock(gate); return amount; }
    export number Read() { return value; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "42\n");
}

TEST_CASE("module exported constants freeze aliases and preserve collection types", "[modules-state]") {
  const auto result = run("link \"./m.clp\" as M; post(M.FIXED[0]); try { M.Change(); } catch (e) { post(\"frozen\"); }", {
    {"./m.clp", "array<number> values = array<number>(7); export const auto FIXED = values; export void Change() { push(values, 8); }"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "7\nfrozen\n");
  CHECK_FALSE(run("link \"./m.clp\" as M; M.FIXED = 1;", {{"./m.clp", "export const number FIXED = 1;"}}).compiled);
}

TEST_CASE("module initialization fails before entry and rejects premature binding reads", "[modules-state]") {
  const auto result = run("link \"./m.clp\" as M; post(\"entry\");", {
    {"./m.clp", "number Read() { return value; } post(Read()); number value = 1;"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); CHECK_FALSE(result.ran);
  CHECK(result.error == "module binding used before initialization"); CHECK(result.output.empty());
}

TEST_CASE("private module observables retain runtime subscriptions", "[modules-state]") {
  const auto result = run("link \"./m.clp\" as M; M.Change(9); post(M.Read());", {
    {"./m.clp", R"(
      observable number value = 0; number observed = 0;
      value.OnChange(func(number changed) -> void { observed = changed; });
      export void Change(number changed) { value = changed; }
      export number Read() { return observed; }
    )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "9\n");
}


TEST_CASE("module defaults use inferred private binding types and shared state cannot be moved", "[modules-state]") {
  const auto result = run("link \"./m.clp\" as M; post(M.Read());", {{"./m.clp", R"(
    def Settings { number Initial = 12; }
    auto settings = Settings {};
    def Counter { number Value = settings.Initial; }
    auto counter = Counter {};
    export number Read() { return counter.Value; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "12\n");
  CHECK_FALSE(run("link \"./m.clp\" as M;", {{"./m.clp", "number value;"}}).compiled);
  CHECK_FALSE(run("link \"./m.clp\" as M;", {{"./m.clp", "number value = 1; export number Take() { return move(value); }"}}).compiled);
}


TEST_CASE("entry functions share inferred file state with callbacks and defaults", "[entry-scope]") {
  const auto result = run(R"(
    def Settings { number Initial = 12; }
    auto settings = Settings {};
    def Counter { number Value = settings.Initial; }
    auto counter = Counter {};
    number Read() { return counter.Value; }
    void Add(number amount) { counter.Value += amount; }
    const auto limit = Read() + 30;
    auto callback = func() -> number { Add(1); return Read(); };
    number Invoke() { return callback(); }
    Add(2); post(Read()); post(Invoke()); post(limit);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "14\n15\n42\n");
}

TEST_CASE("entry initialization follows source order and guards premature calls", "[entry-scope]") {
  const auto result = run(R"(
    number Read() { return value; }
    try { post(Read()); } catch (e) { post("not ready"); }
    number value = 7;
    post(Read());
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "not ready\n7\n");
  CHECK_FALSE(run("number value;").compiled);
  CHECK_FALSE(run("number value = 1; number Take() { return move(value); }").compiled);
}

TEST_CASE("entry locals and parameters shadow file bindings lexically", "[entry-scope]") {
  const auto result = run(R"(
    number value = 10;
    number Read(number value) { return value + 1; }
    number Local() { number value = 2; return value; }
    number Scoped() { if (true) { number value = 3; post(value); } return value; }
    post(Read(4)); post(Local()); post(Scoped()); post(value);
    number index = 99;
    for (index = 0; index < 3; index++) { value += 1; }
    post(index); post(value);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "5\n2\n3\n10\n10\n3\n13\n");
}

TEST_CASE("entry globals share worker state and observables without changing const types", "[entry-scope]") {
  const auto result = run(R"(
    observable number value = 0;
    number observed = 0;
    number gate = mutex();
    value.OnChange(func(number changed) -> void { observed = changed; });
    number Add(number amount) { lock(gate); value += amount; unlock(gate); return amount; }
    number a = thread Add(20); number b = thread Add(22);
    join(a); join(b); post(value); post(observed);
    array<number> items = array<number>(7);
    const auto fixed = items;
    void Change() { push(items, 8); }
    try { Change(); } catch (e) { post("frozen"); }
    post(fixed[0]);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n42\nfrozen\n7\n");
  CHECK_FALSE(run("const auto fixed = 1; void Change() { fixed = 2; }").compiled);
}

TEST_CASE("entry globals are fresh on repeated execution", "[entry-scope]") {
  const auto compiled = clpp::Compiler{}.compile("number value = 0; number Next() { return ++value; } post(Next());");
  REQUIRE(compiled.ok()); clpp::VirtualMachine vm; std::ostringstream out; vm.set_output(out); vm.load(compiled.chunk);
  REQUIRE(vm.run()); REQUIRE(vm.run()); CHECK(out.str() == "1\n1\n");
}


TEST_CASE("generic functions become concrete typed callback values", "[generic-callback]") {
  const auto result = run(R"(
    template <typename T> T Identity(T value) { return value; }
    def Transform<T> = (T) -> T;
    Transform<number> integer = Identity<number>;
    auto text = Identity<string>;
    auto decimal = Identity<float>;
    number Apply(Transform<number> transform, number value) { return transform(value); }
    post(Apply(integer, 42)); post(text("typed")); post(decimal(7) / 2);
    def Player { number Health; }
    auto player = Identity<Player>;
    post(player(Player { Health = 75 }).Health);
    array<Transform<number>> callbacks = array<Transform<number>>(integer);
    post(callbacks[0](9));
    bool less = 1 < 2; auto other = Identity<number>;
    bool greater = other(3) > 2; post(less and greater);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\ntyped\n3.5\n75\n9\ntrue\n");
}

TEST_CASE("generic callback references retain module identities and exports", "[generic-callback]") {
  const auto result = run(R"(
    link "./m.clp" as M;
    def Value { string Different; }
    auto read = M.Identity<M.Value>;
    auto item = read(M.Value { Count = 8 }); post(item.Count);
    auto factory = M.Identity<array<M.Value>>;
    post(factory(array<M.Value>(item))[0].Count);
  )", {{"./m.clp", "\n"
  "    export def Value { number Count; }\n"
  "    export template <typename T> T Identity(T value) { return value; }\n"
  "    template <typename T> T Hidden(T value) { return value; }\n"
  "  "}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "8\n8\n");
  CHECK_FALSE(run("link \"./m.clp\" as M; auto secret = M.Hidden<number>;", {
    {"./m.clp", "template <typename T> T Hidden(T value) { return value; }"}}).compiled);
}

TEST_CASE("concrete generic callbacks enforce arguments results constraints and ambiguity", "[generic-callback]") {
  const std::string identity = "template <typename T> T Identity(T value) { return value; } ";
  for (const auto& source : {
    "auto callback = Identity;", "auto callback = Identity<number,string>;",
    "auto callback = Identity<Missing>;", "auto callback = Identity<number>; callback(\"bad\");",
    "def Wrong = (number) -> string; Wrong callback = Identity<number>;",
    "number Identity(number value) { return value; } auto callback = Identity<number>;"
  }) { INFO(source); CHECK_FALSE(run(identity + source).compiled); }
  const std::string constrained = "template <typename T : number> T Numeric(T value) { return value; } ";
  CHECK(run(constrained + "auto callback = Numeric<number>; post(callback(12));").output == "12\n");
  CHECK_FALSE(run(constrained + "auto callback = Numeric<string>;").compiled);
  CHECK(run("template <typename T> T Identity(T value) { return value; } template <typename U> any Make(U value) { return Identity<U>; }").compiled);
  CHECK_FALSE(run("def Box { template <typename T> T Echo(T value) { return value; } } auto callback = Box.Echo<number>;").compiled);
}

TEST_CASE("generic callback signatures support returned values closures and workers", "[generic-callback]") {
  const auto result = run(R"(
    template <typename T> T Identity(T value) { return value; }
    def Transform = (number) -> number;
    Transform Make() { Transform selected = Identity<number>; return func(number value) -> number { return selected(value); }; }
    auto callback = Make(); post(callback(19));
    number worker = thread callback(23); post(join(worker));
    auto decimal = Identity<float>; number other = thread decimal(7); post(join(other) / 2);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "19\n23\n3.5\n");
}


TEST_CASE("generic closures capture the declaration scoped type and binding", "[generic-capture]") {
  const auto result = run(R"(
    def Getter<T> = () -> T;
    template <typename T> Getter<T> Keep(T value) {
      return func() -> T { return value; };
    }
    auto integer = Keep(42); auto text = Keep("typed");
    post(integer()); post(text());
    def Player { number Health; }
    auto original = Player { Health = 75 };
    auto player = Keep(original); original.Health = 80;
    post(player().Health); post(player() == original);
    auto decimal = Keep<float>(7); post(decimal() / 2);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\ntyped\n80\ntrue\n3.5\n");
}

TEST_CASE("generic closures share mutable cells with sibling and nested captures", "[generic-capture]") {
  const auto result = run(R"(
    def Getter<T> = () -> T;
    def Setter<T> = (T) -> void;
    def Cell<T> { Getter<T> Read; Setter<T> Write; }
    template <typename T> Cell<T> Make(T value) {
      return Cell<T> {
        Read = func() -> T { return value; },
        Write = func(T updated) -> void { value = updated; }
      };
    }
    def Factory<T> = () -> Getter<T>;
    template <typename T> Factory<T> Nested(T value) {
      return func() -> Getter<T> { return func() -> T { return value; }; };
    }
    auto integer = Make(1); auto text = Make("first");
    integer.Write(9); text.Write("second"); post(integer.Read()); post(text.Read());
    auto outer = Nested(12); auto inner = outer(); post(inner());
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\nsecond\n12\n");
}

TEST_CASE("generic closures inherit bounds and reject unsound bodies", "[generic-capture]") {
  const auto result = run(R"(
    def Getter<T> = () -> T;
    template <typename T : number> Getter<T> Counter(T start) {
      return func() -> T { start += 1; return start; };
    }
    auto next = Counter(10); post(next()); post(next());
    template <typename T : number | float> Getter<T> Increment(T value) { return func() -> T { value += 1; return value; }; }
    auto decimal = Increment<float>(7); post(decimal() / 2);
    def Player { number Health; }
    def Reader = () -> number;
    template <typename T : Player> Reader Read(T value) {
      return func() -> number { return value.Health; };
    }
    auto read = Read(Player { Health = 42 }); post(read());
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "11\n12\n4\n42\n");
  for (const auto& source : {
    "def Getter<T> = () -> T; template <typename T> Getter<T> Bad(T value) { return func() -> T { return 1; }; }",
    "def Getter<T> = () -> T; template <typename T> Getter<T> Bad(T value) { return func() -> T { value = 1; return value; }; }",
    "def Getter<T> = () -> T; template <typename T> Getter<T> Bad(T value) { const T fixed = value; return func() -> T { fixed = value; return fixed; }; }",
    "def Getter<T> = () -> T; template <typename T> Getter<T> Bad(T value) { return func() -> T { return value + 1; }; }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("module generic closures keep formals separate from nominal names", "[generic-capture]") {
  const auto result = run("link \"./m.clp\" as M; auto integer = M.Keep(42); auto text = M.Keep(\"typed\"); post(integer()); post(text());", {
    {"./m.clp", R"(
      def T { string Wrong; }
      export def Getter<T> = () -> T;
      export template <typename T> Getter<T> Keep(T value) {
        return func() -> T { return value; };
      }
    )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\ntyped\n");
}


TEST_CASE("generic methods return closures with owner and method type scopes", "[generic-capture]") {
  const auto result = run(R"(
    def Getter<T> = () -> T;
    def Box<T> {
      T Value;
      Getter<T> Reader() { return func() -> T { return @this.Value; }; }
      template <typename U> Getter<U> Keep(U value) { return func() -> U { return value; }; }
    }
    auto box = Box<number> { Value = 7 }; auto reader = box.Reader();
    box.Value = 9; post(reader());
    auto text = box.Keep("typed"); post(text());
    number worker = thread reader(); post(join(worker));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\ntyped\n9\n");
}

TEST_CASE("generic closure scopes cannot leak into unrelated functions", "[generic-capture]") {
  CHECK_FALSE(run(R"(
    def Getter<T> = () -> T; template <typename T> Getter<T> Keep(T value) { return func() -> T { return value; }; }
    auto outside = func(T value) -> T { return value; };
  )").compiled);
  CHECK_FALSE(run(R"(
    def Getter<T> = () -> T;
    template <typename T> Getter<T> Keep(T value) { return func() -> T { return value; }; }
    Getter<string> wrong = Keep(42);
  )").compiled);
  const auto result = run(R"(
    def Getter<T> = () -> T; template <typename T> Getter<T> Keep(T value) { const T fixed = value; return func() -> T { return fixed; }; }
    auto integer = Keep(7); auto text = Keep("typed");
    number first = thread integer(); number second = thread text();
    post(join(first)); post(join(second));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntyped\n");
}

TEST_CASE("contextual lambda parameters use declarations returns and assignments", "[contextual-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T) -> T;
    Transform<number> callback = func(value) -> number { return value + 1; };
    post(callback(4));
    callback = func(value) -> number { return value + 2; }; post(callback(4));
    Transform<string> Make() { return func(value) -> string { return value; }; }
    auto text = Make(); post(text("typed"));
    def Box { Transform<number> Apply; }
    auto box = Box { Apply = func(value) -> number { return value * 2; } };
    post(box.Apply(3));
    box.Apply = func(value) -> number { return value * 3; }; post(box.Apply(3));
    array<Transform<number>> callbacks = array<Transform<number>>(func(value) -> number { return value + 4; });
    post(callbacks[0](3));
    def DefaultBox { Transform<number> Apply = func(value) -> number { return value + 5; }; }
    auto defaults = DefaultBox {}; post(defaults.Apply(3));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "5\n6\ntyped\n6\n9\n7\n8\n");
}

TEST_CASE("contextual callback arguments specialize from independent generic evidence", "[contextual-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T) -> T;
    template <typename T> T Apply(Transform<T> callback, T value) { return callback(value); }
    post(Apply(func(value) -> number { return value + 1; }, 4));
    post(Apply<string>(func(value) -> string { return value; }, "typed"));
    number Invoke(Transform<number> callback) { return callback(8); }
    post(Invoke(func(value) -> number { return value + 2; }));
    def Invoker = (Transform<number>) -> number;
    Invoker invoke = Invoke;
    post(invoke(func(value) -> number { return value + 3; }));
    def Box<T> { T Value; T Apply(Transform<T> callback) { return callback(@this.Value); } }
    auto box = Box<number> { Value = 6 };
    post(box.Apply(func(value) -> number { return value * 2; }));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "5\ntyped\n10\n11\n12\n");
}

TEST_CASE("contextual generic callbacks retain outer bounds and nested captured parameters", "[contextual-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T) -> T;
    def Factory<T> = (T) -> Transform<T>;
    template <typename T> Factory<T> Keep() {
      return func(value) -> Transform<T> { return func(ignored) -> T { return value; }; };
    }
    auto factory = Keep<number>(); auto callback = factory(9); post(callback(3));
    template <typename T: number> Transform<T> Increment(T amount) {
      return func(value) -> T { return value + amount; };
    }
    auto increment = Increment(2); post(increment(4));
    number worker = thread callback(7); post(join(worker));
    def TransformFloat = (float) -> float;
    TransformFloat decimal = func(value) -> float { return value / 2; }; post(decimal(3));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\n6\n9\n1.5\n");
}

TEST_CASE("contextual lambdas reject missing ambiguous and contradictory evidence", "[contextual-callback]") {
  for (const auto& source : {
    "auto callback = func(value) -> number { return value; };",
    "def F = (number) -> number; F callback = func(string value) -> number { return 1; };",
    "def F = (number) -> number; F callback = func(value, other) -> number { return value; };",
    "def F = (number) -> number; F callback = func(value) -> string { return value; };",
    "def F = (number) -> number; F callback = func(value) -> number { value = \"bad\"; return value; };",
    "def F = (number) -> number; def G = (string) -> number; def Either = F | G; Either callback = func(value) -> number { return 1; };",
    "def F<T> = (T) -> T; template <typename T> T Apply(F<T> callback) { return callback(1); } Apply(func(value) -> number { return value; });",
    "def F<T> = (T) -> T; template <typename T> F<T> Bad() { return func(value) -> T { return value + 1; }; }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("contextual callbacks preserve module identities and observable signatures", "[contextual-callback]") {
  const auto result = run(R"(
    link "./callbacks.clp" as M;
    def Player { string Wrong; }
    post(M.Read(func(player) -> number { return player.Health; }));
    observable number value = 0; number seen = 0;
    value.OnChange(func(next) -> void { seen = next; }); value = 7; post(seen);
    def Reader = (M.Player) -> number;
    Reader read = func(player) -> number { return player.Health; };
    post(read(M.Player { Health = 11 }));
  )", {{"./callbacks.clp", R"(
    export def Player { number Health; }
    def Reader = (Player) -> number;
    export number Read(Reader callback) { return callback(Player { Health = 9 }); }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\n7\n11\n");
  CHECK_FALSE(run("observable number value = 0; value.OnChange(func(next) -> void { next = \"bad\"; });").compiled);
}

TEST_CASE("generic function values specialize with enclosing formals and survive returns", "[formal-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T) -> T;
    template <typename T> T Identity(T value) { return value; }
    template <typename U> Transform<U> Make() { return Identity<U>; }
    auto integer = Make<number>(); auto text = Make<string>(); auto decimal = Make<float>();
    post(integer(7)); post(text("typed")); post(decimal(3) / 2);
    def Player { number Health; }
    auto player = Make<Player>(); post(player(Player { Health = 42 }).Health);
    template <typename T> Transform<T> Wrapped(T initial) {
      auto callback = Identity<T>;
      return func(value) -> T { post(callback(initial)); return callback(value); };
    }
    auto wrapped = Wrapped(9); post(wrapped(11));
    number worker = thread integer(13); post(join(worker));
    template <typename T> Transform<array<T>> Collection() { return Identity<array<T>>; }
    auto collection = Collection<number>(); post(collection(array<number>(1,2))[1]);
    def Factory<T> = () -> Transform<T>;
    template <typename U> Factory<U> Nested() { return func() -> Transform<U> { return Identity<U>; }; }
    auto nestedFactory = Nested<string>(); auto nested = nestedFactory(); post(nested("nested"));
    template <typename V> V FromCollection(V value) {
      array<Transform<V>> callbacks = array<Transform<V>>(Identity<V>);
      return callbacks[0](value);
    }
    post(FromCollection(15)); post(FromCollection("collection"));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntyped\n1.5\n42\n9\n11\n13\n2\nnested\n15\ncollection\n");
}

TEST_CASE("formal callback constraints require proof from the enclosing bounds", "[formal-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T) -> T;
    template <typename T: number> T Increment(T value) { return value + 1; }
    template <typename U: number> Transform<U> Make(U value) { return Increment<U>; }
    auto increment = Make(1); post(increment(8));
    def Player { number Health; }
    template <typename T: Player> number Health(T value) { return value.Health; }
    def Reader<T> = (T) -> number;
    template <typename U: Player> Reader<U> Read() { return Health<U>; }
    auto read = Read<Player>(); post(read(Player { Health = 17 }));
    template <typename T> T Identity(T value) { return value; }
    def Box<T> {
      Transform<T> Reader() { return Identity<T>; }
      template <typename U> Transform<U> Other(U value) { return Identity<U>; }
    }
    auto box = Box<number> {}; auto own = box.Reader(); auto other = box.Other("typed");
    post(own(19)); post(other("method"));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\n17\n19\nmethod\n");
  for (const auto& source : {
    "def F<T> = (T)->T; template <typename T: number> T Numeric(T value) { return value; } template <typename U> F<U> Bad() { return Numeric<U>; }",
    "def F<T> = (T)->T; template <typename T: number> T Numeric(T value) { return value; } template <typename U: string> F<U> Bad() { return Numeric<U>; }",
    "template <typename T> T Identity(T value) { return value; } auto callback = Identity<T>;",
    "def F<T> = (T)->T; template <typename T> T Identity(T value) { return value; } template <typename U> F<string> Bad() { return Identity<U>; }",
    "template <typename T> T Identity(T value) { return value; } template <typename U> U Bad(U value) { auto callback = Identity<U>; return callback(\"wrong\"); }",
    "template <typename T> T Identity(T value) { return value; } template <typename U> U Bad(U value) { auto callback = Identity<U>; return callback<number>(value); }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("imported formal callbacks preserve names and generic scope identities", "[formal-callback]") {
  const auto result = run(R"(
    link "./callbacks.clp" as M;
    auto numberCallback = M.Make<number>(); auto textCallback = M.Make<string>();
    post(numberCallback(4)); post(textCallback("typed"));
    auto playerCallback = M.Make<M.Player>(); post(playerCallback(M.Player { Health = 23 }).Health);
  )", {{"./callbacks.clp", R"(
    def U { string Wrong; }
    export def Player { number Health; }
    def Transform<T> = (T)->T;
    template <typename T> T Identity(T value) { return value; }
    export template <typename U> Transform<U> Make() { return Identity<U>; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "4\ntyped\n23\n");
}

TEST_CASE("async function callbacks create typed tasks and preserve scheduling", "[async-callback]") {
  const auto result = run(R"(
    number executions = 0;
    async number Double(number value) { executions += 1; return value * 2; }
    def AsyncTransform = (number) -> task<number>;
    AsyncTransform callback = Double;
    auto pending = callback(4); post(executions); post(await pending); post(executions);
    post(await callback(5));
    auto started = spawn callback(6); post(await started);
    post(parallel(callback(7), callback(8)));
    array<AsyncTransform> callbacks = array<AsyncTransform>(Double);
    post(await callbacks[0](9));
    def Box { AsyncTransform Apply; }
    auto box = Box { Apply = Double }; post(await box.Apply(10));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "0\n8\n1\n10\n12\n[14, 16]\n18\n20\n");
}

TEST_CASE("async closures retain captured cells across return spawn and await", "[async-callback]") {
  const auto result = run(R"(
    def AsyncTransform = (number) -> task<number>;
    AsyncTransform Make(number initial) {
      number total = initial;
      return async func(value) -> number { total += value; return total; };
    }
    auto callback = Make(10); auto pending = callback(2);
    post(await pending); post(await spawn callback(3)); post(await callback(4));
    auto sibling = Make(20); post(await sibling(5));
    async void Nothing() { post("void"); }
    auto nothing = Nothing; await nothing();
    def Getter = () -> task<number>;
    Getter Nested(number value) {
      auto synchronous = func() -> number { return value; };
      return async func() -> number { return synchronous(); };
    }
    auto nested = Nested(42); post(await spawn nested());
    def Delay = (number) -> task<number>;
    number Invoke(Delay callback) { return await callback(8); }
    post(Invoke(async func(value) -> number { return value + 1; }));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "12\n15\n19\n25\nvoid\n42\n9\n");
}

TEST_CASE("async generic callback values preserve nominal and float task payloads", "[async-callback]") {
  const auto result = run(R"(
    def AsyncTransform<T> = (T) -> task<T>;
    template <typename T> async T Identity(T value) { return value; }
    template <typename U> AsyncTransform<U> Make() { return Identity<U>; }
    auto integer = Make<number>(); auto text = Make<string>(); auto decimal = Make<float>();
    post(await integer(7)); post(await text("typed")); post((await decimal(3)) / 2);
    def Player { number Health; }
    auto player = Make<Player>(); post((await player(Player { Health = 23 })).Health);
    template <typename T> AsyncTransform<T> Keep(T initial) {
      return async func(value) -> T { return initial; };
    }
    auto kept = Keep("captured"); post(await spawn kept("other"));
    def Box<T> { AsyncTransform<T> Callback() { return Identity<T>; } }
    auto box = Box<number> {}; auto callback = box.Callback(); post(await callback(31));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntyped\n1.5\n23\ncaptured\n31\n");
}

TEST_CASE("async callbacks propagate observed and unobserved errors", "[async-callback]") {
  const auto result = run(R"(
    def Failure = () -> task<number>;
    Failure Make(string message) { return async func() -> number { report(message); return 0; }; }
    auto fail = Make("callback failure");
    try { post(await fail()); } catch (error) { post(error); }
    try { post(await spawn fail()); } catch (error) { post(error); }
    post("survived");
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "callback failure\ncallback failure\nsurvived\n");
  const auto unobserved = run("auto fail = async func() -> number { report(\"unobserved\"); return 0; }; auto job = spawn fail();");
  REQUIRE(unobserved.compiled); CHECK_FALSE(unobserved.ran); CHECK(unobserved.error == "unobserved");
  const auto isolated = run("number Work(number ignored) { auto callback = async func() -> number { return fs::size(\"missing\"); }; return await spawn callback(); } post(actor(Work, 0));");
  REQUIRE(isolated.compiled); CHECK_FALSE(isolated.ran); CHECK(isolated.error == "sandbox");
}

TEST_CASE("async callback signatures reject unchecked task payloads and synchronous handlers", "[async-callback]") {
  for (const auto& source : {
    "spawn work;",
    "await 1;",
    "spawn 1;",
    "async number Double(number value) { return value; } def F = (number)->number; F callback = Double;",
    "async number Double(number value) { return value; } def F = (number)->task<string>; F callback = Double;",
    "async number Double(number value) { return value; } auto callback = Double; callback(\"bad\");",
    "async number Double(number value) { return value; } auto callback = Double; number value = callback(1);",
    "async number Double(number value) { return value; } auto callback = Double; number worker = thread callback(1);",
    "auto callback = async func(value) -> number { return value; };",
    "observable number value = 0; value.OnChange(async func(next) -> void {});",
    "auto callback = async func(number value) { return value; };",
    "task<string> job = spawn async func() -> number { return 1; }();",
    "def Bad = (number)->task<Missing>; Bad callback = async func(value) -> number { return value; };"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("async callback imports and synchronous task factories keep distinct scheduling", "[async-callback]") {
  const auto result = run(R"(
    link "./async.clp" as M;
    auto identity = M.Identity<M.Player>;
    post((await identity(M.Player { Health = 42 })).Health);
    number invoked = 0;
    async number Double(number value) { return value * 2; }
    task<number> Produce(number value) { invoked += 1; return Double(value); }
    def Callback = (number) -> task<number>;
    Callback producer = Produce; Callback asynchronous = Double;
    auto pending = producer(5); post(invoked); post(await pending); post(await asynchronous(6));
    def Getter = () -> task<number>;
    task<number> Pending(number value) {
      Getter callback = async func() -> number { return value; };
      return callback();
    }
    auto kept = Pending(77);
    for (number index = 0; index < 500; index += 1) { auto garbage = array<number>(index); }
    post(await spawn kept);
  )", {{"./async.clp", R"(
    export def Player { number Health; }
    export template <typename T> async T Identity(T value) { return value; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n1\n10\n12\n77\n");
}

TEST_CASE("bound methods retain receiver identity mutation and factory lifetime", "[method-callback]") {
  const auto result = run(R"(
    def Counter {
      number Value;
      number Add(number amount) { @this.Value += amount; return @this.Value; }
      number Read() { return @this.Value; }
    }
    auto counter = Counter { Value = 10 }; auto add = counter.Add; auto read = counter.Read;
    post(add(2)); counter.Value = 20; post(read());
    counter = Counter { Value = 100 }; post(add(3)); post(counter.Value);
    number created = 0;
    Counter Make() { created += 1; return Counter { Value = 30 }; }
    auto temporary = Make().Add; post(created); post(temporary(4)); post(temporary(5));
    def Getter = () -> number;
    Getter Kept() { auto local = Counter { Value = 42 }; return local.Read; }
    auto kept = Kept(); post(kept()); number worker = thread kept(); post(join(worker));
    def Update = (number)->number;
    array<Update> callbacks = array<Update>(add); post(callbacks[0](1));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "12\n20\n23\n100\n1\n34\n39\n42\n42\n24\n");
}

TEST_CASE("method callbacks and direct async calls preserve virtual dispatch", "[method-callback]") {
  const auto result = run(R"(
    def Base {
      number Value;
      number Read() { return @this.Value; }
      async number Compute(number amount) { @this.Value += amount; return @this.Value; }
    }
    def Child : Base {
      override number Read() { return @this.Value + 100; }
      override async number Compute(number amount) { @this.Value += amount * 2; return @this.Value; }
    }
    Base receiver = Child { Value = 10 }; auto read = receiver.Read; auto compute = receiver.Compute;
    post(read()); auto pending = receiver.Compute(2); post(receiver.Value); post(await pending);
    post(await compute(3)); post(await spawn receiver.Compute(4));
    post(await spawn compute(5)); post(read());
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "110\n10\n14\n20\n28\n38\n138\n");
}

TEST_CASE("bound generic methods specialize owner method and enclosing formals", "[method-callback]") {
  const auto result = run(R"(
    def Box<T> {
      T Value;
      T Read() { return @this.Value; }
      template <typename U> U Echo(U value) { return value; }
      template <typename U> async U Later(U value) { return value; }
    }
    auto box = Box<number> { Value = 7 }; auto read = box.Read; auto echo = box.Echo<string>;
    post(read()); post(echo("typed"));
    auto later = box.Later<float>; post((await later(3)) / 2);
    post((await box.Later<float>(5)) / 2);
    def Transform<T> = (T)->T;
    template <typename V> Transform<V> Select(Box<number> owner) { return owner.Echo<V>; }
    auto integer = Select<number>(box); auto text = Select<string>(box);
    post(integer(11)); post(text("formal"));
    def Derived<T> : Box<T> {}
    auto derived = Derived<string> { Value = "inherited" }; auto inherited = derived.Read; post(inherited());
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntyped\n1.5\n2.5\n11\nformal\ninherited\n");
}

TEST_CASE("bound methods obey access checks callback signatures and constraints", "[method-callback]") {
  const auto result = run("\n"
  "    def Getter = ()->number;\n"
  "    def Secret { private number Hidden() { return 42; } public Getter Expose() { return @this.Hidden; } }\n"
  "    auto secret = Secret {}; auto exposed = secret.Expose(); post(exposed());\n"
  "    def Observer { number Seen = 0; void Changed(number value) { @this.Seen = value; } }\n"
  "    auto observer = Observer {}; observable number value = 0;\n"
  "    value.OnChange(observer.Changed); value = 9; post(observer.Seen);\n"
  "  ");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n9\n");
  for (const auto& source : {
    "def Box { private number Read() { return 1; } } auto box = Box {}; auto read = box.Read;",
    "def Box { number Read(number value) { return value; } } auto box = Box {}; auto read = box.Read; read(\"bad\");",
    "def Box { number Read() { return 1; } } def F = ()->string; auto box = Box {}; F read = box.Read;",
    "def Box { template <typename T> T Echo(T value) { return value; } } auto box = Box {}; auto echo = box.Echo;",
    "def Box { template <typename T: number> T Echo(T value) { return value; } } auto box = Box {}; auto echo = box.Echo<string>;",
    "def Box { number Read() { return 1; } } auto read = Box.Read;",
    "def Box { number Read() { return 1; } } auto box = Box {}; auto read = box.Read<number>;"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("imported bound methods keep generic results and async failures", "[method-callback]") {
  const auto result = run(R"(
    link "./methods.clp" as M;
    auto box = M.Box<M.Player> { Value = M.Player { Health = 42 } };
    auto read = box.Read; post(read().Health);
    auto later = box.Later; post((await spawn later()).Health);
    auto callback = box.Echo<string>; post(callback("typed"));
    def Failure { async number Fail() { report("method failure"); return 0; } }
    auto receiver = Failure {}; auto fail = receiver.Fail;
    try { post(await fail()); } catch (error) { post(error); }
    try { post(await spawn receiver.Fail()); } catch (error) { post(error); }
  )", {{"./methods.clp", R"(
    export def Player { number Health; }
    export def Box<T> {
      T Value;
      T Read() { return @this.Value; }
      async T Later() { return @this.Value; }
      template <typename U> U Echo(U value) { return value; }
    }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n42\ntyped\nmethod failure\nmethod failure\n");
  const auto unobserved = run("def Failure { async number Fail() { report(\"unobserved method\"); return 0; } } auto receiver = Failure {}; auto fail = receiver.Fail; auto job = spawn fail();");
  REQUIRE(unobserved.compiled); CHECK_FALSE(unobserved.ran); CHECK(unobserved.error == "unobserved method");
  CHECK_FALSE(run("def Base { async number Read() { return 1; } } def Child : Base { override number Read() { return 2; } }").compiled);
  CHECK_FALSE(run("def Box { number Read() { return 1; } } auto box = Box {}; auto work = thread box.Read();").compiled);
}

TEST_CASE("typed JSON retains recursive payloads exact numbers and actual booleans", "[typed-json]") {
  const auto result = run(R"(
    link @clpp.json as Json;
    auto value = Json.ParseValue("{\"flag\":true,\"items\":[9007199254740993,1.5,null]}");
    post(value.Type == Json.Kind::Object); post(value.Entries["flag"].Boolean);
    post(value.Entries["items"].Items[0].Integer); post(value.Entries["items"].Items[1].Decimal);
    post(Json.StringifyValue(value));
    auto node = Json.Value { Type = Json.Kind::Array }; push(node.Items, node);
    try { Json.StringifyValue(node); } catch(error) { post(error); }
    post(Json.StringifyValue(Json.ParseValue("false")));
    post(Json.StringifyValue(Json.ParseValue("1.0"))); post(Json.StringifyValue(Json.ParseValue("-0.0")));
    auto malformed = Json.Value { Type = Json.Kind::Integer };
    malformed.Integer = Json.Parse("\"wrong\"");
    try { Json.StringifyValue(malformed); } catch(error) { post(error); }
    auto forged = Json.Parse("[0,\"\",0,0,0,[],{}]");
    try { Json.StringifyValue(forged); } catch(error) { post(error); }
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "true\ntrue\n9007199254740993\n1.5\n{\"flag\":true,\"items\":[9007199254740993,1.5,null]}\nJSON cannot encode cyclic values\nfalse\n1.0\n-0.0\ninvalid JSON integer\ninvalid typed JSON value\n");
  CHECK_FALSE(run("link @clpp.json as Json; auto value=Json.ParseValue(7);").compiled);
  CHECK_FALSE(run("link @clpp.json as Json; auto value=Json.ParseValue(\"[]\"); push(value.Items, 1);").compiled);
}

TEST_CASE("JSON preserves exact integer tokens and rejects malformed Unicode pairs", "[json-integrity]") {
  const auto result = run(R"(
    link @clpp.json as Json;
    post(Json.Stringify(Json.Parse("9007199254740993")));
    post(Json.Stringify(Json.Parse("9223372036854775807")));
    post(Json.Stringify(Json.Parse("-9223372036854775808")));
    post(Json.Valid("9223372036854775808"));
    post(Json.Valid("-9223372036854775809"));
    post(Json.Valid("\"\\uD800\"")); post(Json.Valid("\"\\uDC00\""));
    post(Json.Valid("\"\\uD800\\u0000\""));
    post(Json.Stringify(Json.Parse("\"\\uD83D\\uDE00\"")));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9007199254740993\n9223372036854775807\n-9223372036854775808\nfalse\nfalse\nfalse\nfalse\nfalse\n\"😀\"\n");
}

TEST_CASE("guard supports loop exits without weakening function return checks", "[guard-loop-exit]") {
  const auto valid = run(R"(
    for (auto item in array<optional<number>>(null, 7, null, 9)) {
      guard (item != null) else { continue; }
      post(item);
    }
    for (number item = 0; item < 3; item += 1) { guard (item < 1) else { break; } post(item); }
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran); CHECK(valid.output == "7\n9\n0\n");
  for (const auto& source : {
    "number Read() { for (number i=0; i<2; i+=1) { guard(i>0) else { break; } } }",
    "void Read(bool flag) { guard(flag) else { continue; } } Read(false);",
    "for(number i=0; i<2; i+=1) { guard(i>0) else { if(i>1) { continue; } } }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("composed generic bounds preserve all evidence and compatible member views", "[composed-constraint]") {
  const auto result = run(R"(
    def Base { number Value; number Read() { return @this.Value; } }
    def Derived : Base { number Extra; }
    def Contract = Base & Derived;
    template <typename T> number Read(T value) where T: Base, T: Derived { return value.Read() + value.Extra; }
    template <typename U: Contract> number Forward(U value) { return Read<U>(value); }
    def Reader<T> = (T)->number;
    template <typename V: Contract> Reader<V> Callback() { return Read<V>; }
    auto item = Derived { Value = 7, Extra = 9 }; post(Read(item)); post(Forward(item));
    auto callback = Callback<Derived>(); post(callback(item));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "16\n16\n16\n");
}

TEST_CASE("composed bounds reject incompatible layouts and cyclic numeric proof", "[composed-constraint]") {
  for (const auto& source : {
    "template <typename T> T Read(T value) where T: number, T: string { return value; } post(Read(7));",
    "def Base { number Value; number Read() { return @this.Value; } } def Bad : Base { string Read() { return \"bad\"; } } template <typename T: Base & Base> number Read(T value) { return value.Read(); } post(Read(Bad { Value = 7 }));",
    "template <typename T: T & number> number Read(T value) { return value.Missing; }",
    "template <typename T: number> T Numeric(T value) { return value; } template <typename U: number | string> U Bad(U value) { return Numeric<U>(value); }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("typed where clauses compose with template bounds and module exports", "[composed-constraint]") {
  const auto result = run(R"(
    link "./bounds.clp" as Bounds;
    def Item : Bounds.Base { number Extra; }
    auto item = Item { Value = 5, Extra = 8 };
    post(Bounds.Read(item));
    template <typename T: Bounds.Base> number Read(T value) where T: Bounds.Base & Bounds.Base {
      return Bounds.Read<T>(value);
    }
    post(Read(item));
  )", {{"./bounds.clp", R"(
    export def Base { number Value; }
    export template <typename T: Base> number Read(T value) where T: Base { return value.Value; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "5\n5\n");
}

TEST_CASE("typed where clauses reject missing syntax and preserve every bound", "[composed-constraint]") {
  for (const auto& source : {
    "template <typename T> T Read(T value) where { return value; }",
    "template <typename T> T Read(T value) where T: number, { return value; }",
    "template <typename T> T Read(T value) where T number { return value; }",
    "template <typename T> T Read(T value) where T: { return value; }",
    "template <typename T> T Read(T value) where U: number { return value; }",
    "number Read(number value) where T: number { return value; }",
    "template <typename T: number> T Read(T value) where T: string { return value; } post(Read(7));",
    "template <typename T: number> T Read(T value) where T: string { return value; } post(Read(\"bad\"));",
    "template <typename T> T Read(T value) where T: number & string { return value; } post(Read(7));"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("fieldless interfaces dispatch through distinct views and inherited implementations", "[interfaces]") {
  const auto result = run(R"(
    abstract def Named { abstract string Name(); }
    abstract def Scored { abstract number Score(); }
    def Storage { number Value; number Score() { return @this.Value; } }
    def Player : Named, Storage, Scored { string Name() { return "Ada"; } }
    auto player = Player { Value = 17 };
    Named named = player; Scored scored = player; Storage stored = player;
    post(named.Name()); post(scored.Score()); post(stored.Score());
    def Read = () -> number;
    Read callback = scored.Score; post(callback());
    template <typename T: Scored> number ReadScore(T value) { return value.Score(); }
    post(ReadScore(player));
    template <typename T> string Describe(T value) where T: Named, T: Scored {
      return value.Name() .: ":" .: value.Score();
    }
    post(Describe(player));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "Ada\n17\n17\n17\n17\nAda:17\n");
}

TEST_CASE("boolean values retain distinct runtime identity through collections and JSON", "[runtime-bool]") {
  const auto valid=run(R"(
    link @clpp.json as Json;
    link @clpp.axiom as Axiom;
    array<bool> flags={true, false, 1<2, !true, 1&&2};
    post(flags); post(Json.Stringify(flags));
    auto parsed=Json.Parse("[true,false,1]"); post(parsed);
    post(parsed[0]==true); post(parsed[0]==parsed[2]);
    auto node=Json.ParseValue("true"); post(node.Boolean==true); post(Json.StringifyValue(node));
    post(Axiom.IsFinite(7));
    signal<bool> changed=signal<bool>();
    changed::Connect(func(bool value) -> void { post(Json.Stringify(value)); }); changed::Fire(true);
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output=="[true, false, true, false, true]\n[true,false,true,false,true]\n[true, false, 1]\ntrue\nfalse\ntrue\ntrue\ntrue\ntrue\n");
  auto value=clpp::Value::boolean_of(true);
  CHECK(value.is_boolean()); CHECK_FALSE(value.is_number()); CHECK_FALSE(value.same_value(clpp::Value::integer_of(1)));
  auto copy=value; CHECK(copy.is_boolean()); CHECK(copy.same_value(value));
  copy.set_number(1); CHECK(copy.is_number()); CHECK_FALSE(copy.is_boolean());
  const auto guards=run(R"(
    gfx::Bold(true);
    try { gfx::Bold(1); } catch(error) { post("bool required"); }
    try { gfx::Canvas(true, 8); } catch(error) { post("number required"); }
  )");
  INFO(guards.error); INFO(guards.detail); REQUIRE(guards.compiled); REQUIRE(guards.ran);
  CHECK(guards.output=="bool required\nnumber required\n");
}

TEST_CASE("constexpr uses typed or inferred constant declarations", "[typed-constexpr]") {
  const auto valid=run(R"(
    link "./constants.clp" as Constants;
    constexpr number exact=9007199254740993+2;
    constexpr float fraction=7.0/2;
    constexpr float promoted=7/2;
    constexpr auto inferred=8*2;
    def Count=number;
    constexpr Count count=6*7;
    constexpr bool enabled=true;
    constexpr string label="typed";
    constexpr optional<number> absent=null;
    post(exact); post(fraction); post(promoted); post(inferred); post(count); post(Constants.LIMIT);
    post(enabled); post(label); post(absent);
  )", {{"./constants.clp", "export constexpr number LIMIT=6*7;"}});
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output=="9007199254740995\n3.5\n3\n16\n42\n42\ntrue\ntyped\nnull\n");
  for(const auto& source : {
    "constexpr old=2+2;",
    "constexpr number invalid=1.5;",
    "constexpr number invalid=true+1;",
    "constexpr number invalid=null;",
    "constexpr number invalid=1/0;",
    "constexpr number invalid=9223372036854775807+1;",
    "constexpr number fixed=7; fixed=8;",
    "number source=7; constexpr number invalid=source;"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("calls require positional arguments while initializers name fields", "[explicit-arguments][positional-calls]") {
  for (const auto& source : {
    "number Inc(number value) { return value+1; } post(Inc(value: 4));",
    "def Item { number Read(number value) { return value; } } auto item=Item {}; post(item.Read(value: 7));",
    "def Item { number Value; } auto item=Item(Value: 7);",
    "template <typename T> T Read(T value) { return value; } post(Read<number>(value: 7));",
    "link @clpp.axiom as Axiom; post(Axiom.Round(value: 1.5));"
  }) {
    INFO(source); const auto invalid=run(source); CHECK_FALSE(invalid.compiled);
    CHECK(invalid.error.find("named call arguments are not supported") != std::string::npos);
  }
  const auto valid = run(R"(
    def Options { string Name; number Count=1; }
    string Describe(Options options) { return options.Name .: ":" .: options.Count; }
    number Inc(number value) { return value+1; }
    auto callback=Inc;
    post(callback(4));
    post(Describe(Options { Count=5, Name="Ada" }));
    post(Describe(Options { Name="Lin" }));
    def Box { number Read(number value) { return value; } }
    post(Box {}.Read(7));
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "5\nAda:5\nLin:1\n7\n");
}

TEST_CASE("function names reject overloads independently of arity and generics", "[explicit-arguments][no-overloads]") {
  for (const auto& source : {
    "number Read() { return 1; } number Read(number value) { return value; }",
    "number Read(number value) { return value; } number Read() { return 1; }",
    "async number Read() { return 1; } number Read(number value) { return value; }",
    "template <typename T> T Read(T value) { return value; } number Read() { return 1; }",
    "number Read() { return 1; } template <typename T> T Read(T value) { return value; }",
    "def Item {} number Item::Read() { return 1; } number Item::Read(number value) { return value; }",
    "def Item { number Read() { return 1; } } number Item::Read(number value) { return value; }",
    "number Read() { return 1; } number Read(number value) { return value; }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
  const auto valid = run(R"(
    link "./left.clp" as Left; link "./right.clp" as Right;
    def First { number Read() { return 7; } }
    def Second { number Read(number value) { return value; } }
    post(Left.Read()); post(Right.Read(9)); post(First {}.Read()); post(Second {}.Read(11));
  )", {{"./left.clp", "export number Read() { return 5; }"},
        {"./right.clp", "export number Read(number value) { return value; }"}});
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "5\n9\n7\n11\n");
  CHECK_FALSE(run("link \"./bad.clp\" as Bad;", {{"./bad.clp", "export number Read() { return 1; } export number Read(number value) { return value; }"}}).compiled);
}

TEST_CASE("standard libraries expose distinct operations instead of overloads", "[no-overloads][native-arguments]") {
  const auto valid = run(R"(
    link @clpp.axiom as Axiom;
    link @clpp.gfx as Gfx;
    post(Axiom.Round(1.55)); post(Axiom.RoundDigits(1.234, 2));
    post(Axiom.IsFinite(Axiom.GaussianWithParameters(0, 0)));
    post(Axiom.OrthonormalUp(Vector3(0, 0, -2), Vector3(0, 1, 0)).z);
    Gfx.Canvas(16, 16); Gfx.Clear(0);
    Gfx.RectLine(0, 0, 8, 8, 255);
    Gfx.RectLineWithThickness(1, 1, 8, 8, 2, 255);
    Gfx.CircleLine(8, 8, 3, 255);
    Gfx.CircleLineWithThickness(8, 8, 3, 2, 255);
    Gfx.Line(0, 0, 15, 15, 255);
    Gfx.LineWithThickness(0, 15, 15, 0, 2, 255);
    post(Gfx.Width());
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "2\n1.23\ntrue\n-1\n16\n");
  for (const auto& source : {
    "link @clpp.axiom as Axiom; post(Axiom.Round(1.234, 2));",
    "link @clpp.axiom as Axiom; post(Axiom.Gaussian(0, 1));",
    "link @clpp.axiom as Axiom; post(Axiom.Orthonormal(Vector3(0,0,1), Vector3(0,1,0)));",
    "link @clpp.gfx as Gfx; Gfx.Line(0,0,1,1,2,255);"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("external methods bind typed receivers and preserve dispatch", "[external-method]") {
  const auto valid = run(R"(
    def Player { string Name; private number Health=100; }
    void Player::Heal(number amount) { @this.Health += amount; }
    number Player::Read() { return @this.Health; }
    Player Player::Identity() { return @this; }
    def Better : Player {}
    number Better::Read() { return 200; }
    auto player=Player { Name="Ada" }; player.Heal(10);
    post(player.Read()); post(player.Identity()==player);
    auto callback=player.Read; post(callback());
    Player view=Better { Name="Rui" }; post(view.Read());
    def Box<T> { T Value; }
    T Box::Read() { return @this.Value; }
    template <typename U> U Box::Echo(U value) { return value; }
    auto box=Box<string> { Value="typed" }; post(box.Read()); post(box.Echo(42));
    async T Box::Later() { return @this.Read(); }
    post(await box.Later());
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "110\ntrue\n110\n200\ntyped\n42\ntyped\n");
  const auto imported = run(R"(
    link "./player.clp" as Players;
    auto player=Players.Player { Value=7 };
    post(player.Read());
  )", {{"./player.clp", R"(
    export def Player { number Value; }
    number Player::Read() { return @this.Value; }
  )"}});
  INFO(imported.error); INFO(imported.detail); REQUIRE(imported.compiled); REQUIRE(imported.ran);
  CHECK(imported.output == "7\n");
  for (const auto& source : {
    "number Missing::Read() { return 7; }",
    "def Player {} number Player::Read() { return @this.Missing; }",
    "def Player {} number Player::Read() { return 7; } number Player::Read() { return 8; }",
    "def Box<T> {} template <typename T> T Box::Read(T value) { return value; }",
    "def Player { private number Value=7; } number Player::Read() { return @this.Value; } auto player=Player {}; post(player.Value);",
    "post(@this);"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("array brace initializers use checked type context", "[array-initializer]") {
  const auto valid = run("\n"
  "    def Numbers = array<number>;\n"
  "    Numbers values = {10, 20, 30};\n"
  "    array<array<number>> nested = {{1, 2}, {3}};\n"
  "    array<float> decimal = {7, 2.5};\n"
  "    def Options<T> { array<T> Values = {}; }\n"
  "    template <typename T> array<T> Pair(T first, T second) { return {first, second}; }\n"
  "    number Sum(array<number> items) { number total=0; for(number item in items) { total+=item; } return total; }\n"
  "    template <typename T> T Last(array<T> items, T fallback) { return len(items)>0 ? items[len(items)-1] : fallback; }\n"
  "    def Transform = (number)->number;\n"
  "    array<Transform> operations = {func(value) -> number { return value+1; }};\n"
  "    def Owner { array<number> Values; }\n"
  "    auto owner=Owner { Values={4, 5} };\n"
  "    values = {9}; owner.Values = {8};\n"
  "    post(Sum({1, 2, 3})); post(Sum(Pair(4, 5))); post(values[0]);\n"
  "    post(nested[0][1]); post(decimal[0]/2); post(operations[0](6));\n"
  "    post(owner.Values[0]); post(len(Options<string> {}.Values));\n"
  "    post(Last({4, 5}, 0)); post(Last<number>({}, 42));\n"
  "  ");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "6\n9\n9\n2\n3.5\n7\n8\n0\n5\n42\n");
  const auto imported = run(R"(
    link "./items.clp" as Items;
    array<Items.Item> items={Items.Item { Value=7 }};
    post(Items.Read(items)); post(Items.Read({Items.Item { Value=9 }}));
    template <typename T> array<array<T>> Nest(T item) { return {{item}, {}}; }
    post(Nest("typed")[0][0]);
  )", {{"./items.clp", R"(
    export def Item { number Value; }
    export number Read(array<Item> items) { return items[0].Value; }
  )"}});
  INFO(imported.error); INFO(imported.detail); REQUIRE(imported.compiled); REQUIRE(imported.ran);
  CHECK(imported.output == "7\n9\ntyped\n");
  for (const auto& source : {
    "array<number> items={1, \"bad\"};",
    "array<number> items={1.5};",
    "array<array<number>> items={{1}, {\"bad\"}};",
    "number item={1};",
    "dictionary<string,number> items={1,2};",
    "auto items={1,2};",
    "array<number> items={1 2};",
    "array<number> items={1,;",
    "void Read(array<number> items) {} Read({\"bad\"});",
    "template <typename T> array<T> Bad() { return {1}; } post(Bad<string>());"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("qualified calls cannot bypass abstract dispatch or private access", "[interfaces][method-access]") {
  for (const auto& source : {
    "abstract def Reader { abstract number Read(); } def Item : Reader { number Read() { return 7; } } auto item=Item {}; post(Reader.Read(item));",
    "abstract def Reader<T> { abstract T Read(); } def Item : Reader<number> { number Read() { return 7; } } auto item=Item {}; post(Reader.Read<number>(item));",
    "abstract def Reader { abstract async number Read(); } def Item : Reader { async number Read() { return 7; } } auto item=Item {}; post(await Reader.Read(item));",
    "def Item { private number Secret() { return 7; } } auto item=Item {}; post(Item.Secret(item));",
    "def Item { private number Secret() { return 7; } } auto item=Item {}; auto worker=thread Item.Secret(item); join(worker);",
    "def Item { private number Secret() { return 7; } } auto item=Item {}; post(actor(Item.Secret, item));",
    "abstract def Reader { abstract number Read(); } def Item : Reader { number Read() { return 7; } } auto item=Item {}; post(actor(Reader.Read, item));",
    "def Item { private number Secret() { return 7; } } def Child : Item { number Leak() { return Item.Secret(@this); } } post(Child {}.Leak());"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
  const auto valid = run("\n"
  "    abstract def Reader { abstract number Read(); }\n"
  "    def Item : Reader {\n"
  "      private number Secret() { return 7; }\n"
  "      number Read() { return Item.Secret(@this); }\n"
  "      number FromClosure() { auto read=func() -> number { return Item.Secret(@this); }; return read(); }\n"
  "    }\n"
  "    auto item=Item {}; Reader view=item;\n"
  "    post(view.Read()); post(item.FromClosure());\n"
  "  ");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran);
  CHECK(valid.output == "7\n7\n");
  const auto imported = run(R"(
    link "./item.clp" as Item;
    auto value=Item.Box {};
    post(Item.Box.Secret(value));
  )", {{"./item.clp", "export def Box { private number Secret() { return 7; } }"}});
  CHECK_FALSE(imported.compiled);
}

TEST_CASE("generic interface views preserve specialization and diamond contracts", "[interfaces]") {
  const auto result = run(R"(
    abstract def Readable<T> { abstract T Read(); }
    abstract def Left<U> : Readable<array<U>> { }
    abstract def Right<U> : Readable<array<U>> { }
    def Storage<T> { T Value; }
    def Box<U> : Left<U>, Right<U>, Storage<array<U>> {
      array<U> Read() { return @this.Value; }
    }
    auto box = Box<number> { Value = array<number>(7, 9) };
    Readable<array<number>> view = box; post(view.Read()[1]);
    template <typename T> T ReadValue(Readable<T> value) { return value.Read(); }
    post(ReadValue(box)[0]);
    template <typename T> array<number> Both(T value) where T: Readable<array<number>>, T: Storage<array<number>> {
      auto callback = value.Read;
      post(value.Value[0]); return callback();
    }
    post(Both(box)[1]);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9\n7\n7\n9\n");
}

TEST_CASE("interface contracts reject missing implementations and incompatible views", "[interfaces]") {
  for (const auto& source : {
    "abstract def Readable { abstract number Read(); } def Missing : Readable { }",
    "abstract def Readable { abstract number Read(); } def Bad : Readable { string Read() { return \"bad\"; } }",
    "abstract def Readable<T> { abstract T Read(); } def Box : Readable<number> { number Read() { return 7; } } Readable<string> view = Box {};",
    "abstract def Readable { abstract number Read(); } auto value = Readable {};",
    "def Left { number X; } def Right { number Y; } def Both : Left, Right { }",
    "abstract def Readable { abstract number Read(); } def Similar { number Read() { return 7; } } Readable view = Similar {};",
    "abstract def First { abstract number Read(); } abstract def Second { abstract string Read(); } def Both : First, Second { number Read() { return 7; } }",
    "abstract def Readable { abstract number Read(); } def Bad : Readable { private number Read() { return 7; } }",
    "abstract def Bad { private abstract number Read(); }",
    "abstract def Bad { final abstract number Read(); }",
    "abstract def Readable<T> { abstract T Read(); } abstract def Left : Readable<number> {} abstract def Right : Readable<string> {} abstract def Bad : Left, Right {}"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("exported async interface calls and method callbacks preserve task results", "[interfaces][async-callback]") {
  const auto result = run(R"(
    link "./contract.clp" as Contract;
    def Storage { number Value; }
    def Reader : Storage, Contract.Reader<number> {
      async number Read() { return @this.Value; }
    }
    Contract.Reader<number> view = Reader { Value = 23 };
    post(await view.Read());
    def Callback = () -> task<number>;
    Callback callback = view.Read; post(await callback());
  )", {{"./contract.clp", "export abstract def Reader<T> { abstract async T Read(); }"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "23\n23\n");
}

TEST_CASE("event Wait suspends async frames and resumes typed payloads once", "[event-wait]") {
  const auto result = run(R"(
    auto changed = signal<number>();
    async number Listen(signal<number> event) {
      number before = 1;
      number value = event::Wait();
      return before + value;
    }
    auto listener = spawn Listen(changed);
    post(10); changed::Fire(41);
    post(await listener); post(await listener);
    async number Silent(signal<> event) { event::Wait(); return 7; }
    auto silent = signal<>(); auto waiting = spawn Silent(silent);
    silent::Fire(); post(await waiting);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "10\n42\n42\n7\n");
}

TEST_CASE("event Wait preserves heterogeneous payload types and parallel dependencies", "[event-wait]") {
  const auto result = run(R"(
    auto changed = signal<number, string, bool>();
    async number Listen(signal<number, string, bool> event) {
      tuple<number, string, bool> payload = event::Wait();
      post(payload[1]); post(payload[2]); return payload[0];
    }
    async number Send(signal<number, string, bool> event) { event::Fire(23, "ready", true); return 7; }
    async number Both(signal<number, string, bool> event) {
      auto results = parallel(Listen(event), Send(event));
      return results[0] + results[1];
    }
    post(await Both(changed));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "ready\ntrue\n30\n");
}

TEST_CASE("event Wait retains async closures traps and multiple listeners", "[event-wait]") {
  const auto result = run(R"(
    def Listener = (signal<number>) -> task<number>;
    Listener Make() {
      number total = 0;
      return async func(signal<number> event) -> number { total += event::Wait(); return total; };
    }
    async number Safe(signal<number> event) {
      try { number value = event::Wait(); return 10 / value; } catch (error) { return 7; }
    }
    auto event = signal<number>(); auto callback = Make();
    auto first = spawn callback(event); auto second = spawn Safe(event);
    event::Fire(0); post(await first); post(await second);
    auto third = spawn callback(event); event::Fire(5); post(await third);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "0\n7\n5\n");
}

TEST_CASE("event Wait cooperates with native producers and contended task mutexes", "[event-wait]") {
  const auto result = run(R"(
    async number Read(signal<number> event, number gate) {
      lock(gate); number value = event::Wait(); unlock(gate); return value;
    }
    async number Locked(number gate) { lock(gate); return 7; }
    number Emit(signal<number> event) { event::Fire(31); return 1; }
    auto event = signal<number>(); number gate = mutex();
    auto first = spawn Read(event, gate); auto second = spawn Locked(gate);
    auto producer = thread Emit(event);
    post(await first); post(await second); post(join(producer));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "31\n7\n1\n");
}

TEST_CASE("cooperative futures can be observed through explicit native worker boundaries", "[event-wait]") {
  const auto result = run(R"(
    async number Compute() { number value=0; for (auto index in 2000) { value += 1; } return value; }
    number Read(task<number> work) { return await work; }
    task<number> Make() { return spawn Compute(); }
    auto work = spawn Compute();
    auto reader = thread Read(work); post(join(reader)); post(await work);
    auto factory = thread Make(); auto returned = join(factory); post(await returned);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "2000\n2000\n2000\n");
}

TEST_CASE("actor task factories retain isolation after returning lazy futures", "[event-wait][async-callback]") {
  const auto result = run(R"(
    async number Probe(number value) { return fs::size("missing"); }
    task<number> Make(number value) { return Probe(value); }
    auto pending = actor(Make, 0); post(await pending);
  )");
  INFO(result.error); REQUIRE(result.compiled); CHECK_FALSE(result.ran); CHECK(result.error == "sandbox");
  for (const auto& source : {
    "async number Work(number value) { return value; } post(actor(Work,1));",
    "number Work(number value) { return value; } post(actor(Work,\"bad\"));",
    "number Work(number value) { return value; } string result = actor(Work,1);"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("actor Wait can resume from another cooperative task on its caller", "[event-wait]") {
  const auto result = run(R"(
    def Channels { signal<number> Data; signal<> Ready; }
    async number Emit(Channels channels) {
      channels.Ready::Wait(); channels.Data::Fire(2000); return 1;
    }
    async number Listen(signal<number> event) { return event::Wait(); }
    number Wait(Channels channels) {
      auto waiting = spawn Listen(channels.Data);
      channels.Ready::Fire(); return await waiting;
    }
    auto channels = Channels { Data = signal<number>(), Ready = signal<>() };
    auto producer = spawn Emit(channels);
    post(actor(Wait, channels)); post(await producer);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "2000\n1\n");
}

TEST_CASE("event Wait rejects synchronous use invalid payload access and unresolved waits", "[event-wait]") {
  for (const auto& source : {
    "auto event = signal<number>(); event::Wait();",
    "number Listen(signal<number> event) { return event::Wait(); }",
    "async number Listen(signal<number> event) { return event::Wait(7); }",
    "async number Listen(signal<number,string> event) { auto payload = event::Wait(); return payload[1]; }",
    "async number Listen(signal<number,string> event) { auto payload = event::Wait(); return payload[2]; }",
    "async number Listen(signal<number,string> event) { auto payload = event::Wait(); number index=0; return payload[index]; }"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
  const auto unresolved = run("auto event = signal<number>(); async number Listen(signal<number> event) { return event::Wait(); } post(await Listen(event));");
  INFO(unresolved.error); REQUIRE(unresolved.compiled); CHECK_FALSE(unresolved.ran);
  CHECK(unresolved.error == "async tasks are waiting with no event producer");
}

TEST_CASE("function parameter defaults are rejected while def options retain field defaults", "[explicit-arguments]") {
  for (const auto& source : {
    "number Add(number value = 1) { return value; }",
    "template <typename T> T Identity(T value = 1) { return value; }",
    "def Box { number Read(number value = 1) { return value; } }",
    "auto callback = func(number value = 1) -> number { return value; };",
    "number Read(number value) { return value; } post(Read());",
    "number Add(number left, number right) { return left + right; } post(Add(left: 1, left: 2));"
  }) {
    INFO(source); const auto compiled = clpp::Compiler{}.compile(source); CHECK_FALSE(compiled.ok()); CHECK(compiled.chunk.code.empty());
  }
  const auto imported = run("link \"./old.clp\" as Old;", {{"./old.clp", "export number Read(number value = 1) { return value; }"}});
  CHECK_FALSE(imported.compiled);
  const auto valid = run("def Options { number Value = 7; } number Read(Options options) { return options.Value; } post(Read(Options {})); post(Read(Options { Value = 9 }));");
  INFO(valid.error); REQUIRE(valid.compiled); REQUIRE(valid.ran); CHECK(valid.output == "7\n9\n");
}

TEST_CASE("native interfaces require explicit arguments and GUI options are typed", "[explicit-arguments]") {
  for (const auto& source : {
    "link @clpp.input as Input; Input.Click();",
    "link @clpp.audio as Audio; Audio.Tone(440, 0.01);",
    "link @clpp.audio as Audio; Audio.Play(1);",
    "link @clpp.gui as Gui; Gui.Text();",
    "link @clpp.gui as Gui; Gui.Text(Gui.TextOptions { size = \"bad\" });",
    "link @clpp.gui as Gui; Gui.Text(Gui.TextOptions { unknown = 1 });"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
  const auto valid = run(R"(
    link @clpp.gui as Gui;
    auto first = Gui.FrameOptions {}; auto second = Gui.FrameOptions {};
    push(first.children, Gui.Text(Gui.TextOptions { text = "typed" }));
    post(len(first.children)); post(len(second.children)); post(Gui.TextOptions {}.visible);
    auto node = Gui.Frame(first); post(len(node) > 0);
  )");
  INFO(valid.error); INFO(valid.detail); REQUIRE(valid.compiled); REQUIRE(valid.ran); CHECK(valid.output == "1\n0\ntrue\ntrue\n");
}

TEST_CASE("optional composite assignment preserves owned type parts and exact integer metadata", "[sanitizer-regression]") {
  const auto result = run(R"(
    const number limit = 9223372036854775807;
    optional<array<number>> items = array<number>(7);
    def Apply = (number)->number;
    optional<Apply> callback = func(number value) -> number { return value; };
    post(limit);
    if (items != null) { post(items[0]); }
    if (callback != null) { post(callback(9)); }
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "9223372036854775807\n7\n9\n");
}

TEST_CASE("extreme audio handles fail safely without native integer conversions", "[sanitizer-regression][audio]") {
  const auto result = run(R"(
    link @clpp.audio as Audio;
    try { Audio.Play(9223372036854775807, 1, 0, 1, false); } catch (error) { post("play rejected"); }
    try { Audio.Duration(9223372036854775807); } catch (error) { post("duration rejected"); }
    Audio.Stop(-9223372036854775808);
    post(Audio.IsPlaying(-9223372036854775808));
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "play rejected\nduration rejected\nfalse\n");
}

TEST_CASE("native input and sleep reject oversized arguments before desktop effects", "[sanitizer-regression][input]") {
  const auto result = run(R"(
    link @clpp.input as Input;
    link @clpp.time as Time;
    try { Input.Move(9223372036854775807, 0); } catch (error) { post("move rejected"); }
    try { Input.Scroll(9223372036854775807); } catch (error) { post("scroll rejected"); }
    try { Input.Pixel(9223372036854775807, 0); } catch (error) { post("pixel rejected"); }
    try { Time.Sleep(1e300); } catch (error) { post("sleep rejected"); }
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "move rejected\nscroll rejected\npixel rejected\nsleep rejected\n");
}

TEST_CASE("compiled diagnostics own spans after analysis and result destruction", "[diagnostic-lifetime]") {
  clpp::CompileResult copied;
  {
    std::string source = "link @clpp.gui as Gui; Gui.Text(Gui.TextOptions { unknown = 1 });";
    const auto original = clpp::Compiler{}.compile(source);
    REQUIRE_FALSE(original.ok());
    REQUIRE(original.diagnostic_text);
    copied = original;
  }
  const auto moved = std::move(copied);
  for (std::size_t index = 0; index < moved.diagnostics.size(); ++index) {
    CHECK(moved.diagnostics[index].span == (*moved.diagnostic_text)[index]);
    CHECK(moved.diagnostics[index].span.data() == (*moved.diagnostic_text)[index].data());
  }
  clpp::ClirEmitResult emitted;
  {
    const auto analysis = clpp::analyze_program("link @clpp.gui as Gui; Gui.Text(Gui.TextOptions { unknown = 1 });", {});
    emitted = clpp::emit_clir(analysis);
    REQUIRE_FALSE(emitted.ok());
  }
  REQUIRE(emitted.diagnostic_text);
  for (std::size_t index = 0; index < emitted.diagnostics.size(); ++index) {
    CHECK(emitted.diagnostics[index].span == (*emitted.diagnostic_text)[index]);
    CHECK(emitted.diagnostics[index].span.data() == (*emitted.diagnostic_text)[index].data());
  }
}

TEST_CASE("generic field defaults retain declaration formals and fresh resources", "[generic-default]") {
  const auto result = run(R"(
    def Defaults<T> { array<T> Items = array<T>(); dictionary<string,T> Named = dictionary<string,T>(); signal<T> Changed = signal<T>(); optional<T> Maybe = null; }
    auto numbers = Defaults<number> {}; auto strings = Defaults<string> {}; auto other = Defaults<number> {};
    push(numbers.Items, 7); push(strings.Items, "text"); numbers.Named["key"] = 9;
    post(numbers.Items[0]); post(strings.Items[0]); post(len(other.Items)); post(numbers.Named["key"]);
    numbers.Changed::Connect(func(value) -> void { post(value); }); numbers.Changed::Fire(11); other.Changed::Fire(13);
    post(numbers.Maybe == null);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntext\n0\n9\n11\ntrue\n");
}

TEST_CASE("generic field default callbacks specialize at each construction", "[generic-default]") {
  const auto result = run(R"(
    template <typename T> T Identity(T value) { return value; }
    def Transform<T> = (T)->T; def Box<T> { Transform<T> Apply = func(value) -> T { return value; }; Transform<T> Named = Identity<T>; }
    auto numbers = Box<number> {}; auto strings = Box<string> {};
    post(numbers.Apply(7)); post(strings.Apply("text")); post(numbers.Named(9));
    template <typename U> Box<U> Make() { return Box<U> {}; } auto made = Make<string>(); post(made.Named("made"));
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\ntext\n9\nmade\n");
}

TEST_CASE("provisional generic field callbacks do not suppress unrelated contextual evidence", "[generic-default][contextual-callback]") {
  const auto result = run(R"(
    def Transform<T> = (T)->T;
    signal<number> changed;
    changed~>Connect(func(value) -> void { post(value); });
    def Defaults<T> {
      array<T> Items = array<T>();
      Transform<T> Apply = func(value) -> T { return value; };
    }
    changed::Fire(7);
    auto value = Defaults<number> {};
    post(value.Apply(8));
  )");
  INFO(result.error); INFO(result.detail);
  REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\n8\n");
  const auto invalid = run(R"(
    def Transform<T> = (T)->T;
    signal<number> changed;
    changed~>Connect(func(value) -> void { post(value); });
    def Defaults<T> { Transform<T> Apply = func(value) -> T { return 1; }; }
  )");
  CHECK_FALSE(invalid.compiled);
}

TEST_CASE("inherited generic defaults specialize their declaring base scope", "[generic-default]") {
  const auto result = run(R"(
    def Transform<T> = (T)->T; def Base<T> { array<T> Items = array<T>(); Transform<T> Apply = func(value) -> T { return value; }; }
    def Derived<U> : Base<array<U>> { signal<U> Changed = signal<U>(); }
    auto value = Derived<number> {}; push(value.Items, array<number>(7));
    post(value.Items[0][0]); post(value.Apply(array<number>(9))[0]);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\n9\n");
}

TEST_CASE("generic defaults preserve module identity and declaration globals", "[generic-default]") {
  const auto result = run(R"(
    link "./defaults.clp" as M;
    def Player { string Health; }
    template <typename T> number Build() { number initial = 99; auto box = M.Box<T> {}; return box.Value; }
    post(Build<string>());
    auto box = M.Box<M.Player> {}; auto item = box.Apply(M.Player { Health = 7 }); post(item.Health);
    def Concrete : M.Box<number> {} auto concrete = Concrete {}; post(concrete.Value); post(concrete.Apply(9));
  )", {{"./defaults.clp", R"(
    number initial = 12;
    export def Player { number Health; }
    def Transform<T> = (T)->T;
    export def Box<T> { number Value = initial; array<T> Items = array<T>(); Transform<T> Apply = func(value) -> T { return value; }; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "12\n7\n12\n9\n");
}

TEST_CASE("generic defaults reject invalid and recursive declarations and incompatible uses", "[generic-default]") {
  for (const auto& source : {
    "def Box<T> { array<T> Items = array<Missing>(); }",
    "def Box<T> { T Value = missing; }",
    "def Transform<T> = (T)->T; def Box<T> { Transform<T> Apply = func(value) -> T { return 1; }; }",
    "def Box<T> { Box<T> Child = Box<T> {}; }",
    "def Box<T> { T Value = 1; } auto box = Box<string> {};",
    "def Box<T> { array<T> Items = array<T>(); } auto box = Box<number> {}; push(box.Items, \"bad\");",
    "def Transform<T> = (T)->T; def Box<T> { Transform<T> Apply = func(value) -> T { return value; }; } auto box = Box<number> {}; box.Apply(\"bad\");"
  }) {
    INFO(source); CHECK_FALSE(run(source).compiled);
  }
  const auto result = run("def Box<T> { T Value = 1; } auto a = Box<number> {}; auto b = Box<float> {}; post(a.Value); post(b.Value + 0.5); auto text = Box<string> { Value = \"explicit\" }; post(text.Value);");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran); CHECK(result.output == "1\n1.5\nexplicit\n");
}

TEST_CASE("observable handles disconnect in order and preserve assigned state on errors", "[observable-connection]") {
  const auto result = run(R"(
    observable number value = 0;
    connection second = value::OnChange(func(next) -> void {}); second::Disconnect();
    auto first = value.OnChange(func(next) -> void { post(next); second::Disconnect(); });
    second = value.OnChange(func(next) -> void { post(999); });
    value = 1; post(first.Connected); post(second.Connected);
    first::Disconnect(); first::Disconnect(); value = 2;
    auto failure = value.OnChange(func(next) -> void { report("changed"); });
    auto last = value.OnChange(func(next) -> void { post(999); });
    try { value = 3; } catch(error) { post(error); }
    post(value); failure::Disconnect(); value = 4; last::Disconnect();
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "1\ntrue\nfalse\nchanged\n3\n999\n");
  CHECK_FALSE(run("observable number value = 0; number handle = value.OnChange(func(next) -> void {});").compiled);
}

TEST_CASE("observable snapshots defer new subscriptions and returned handles retain captures", "[observable-connection]") {
  const auto result = run(R"(
    observable number value = 0;
    auto first = value.OnChange(func(next) -> void {
      post(next); value.OnChange(func(later) -> void { post(later + 10); });
    });
    value = 1; value = 2; first::Disconnect(); value = 3;
    connection Make() { number captured = 40; return value.OnChange(func(next) -> void { post(captured + next); }); }
    auto captured = Make(); value = 4; captured::Disconnect(); value = 5;
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "1\n2\n12\n13\n13\n14\n14\n44\n15\n15\n");
}

TEST_CASE("module observable handles disconnect bound receivers across workers", "[observable-connection]") {
  const auto result = run(R"(
    link "./observable.clp" as M;
    auto handle = M.Listen(); auto worker = thread M.Change(7); join(worker);
    post(M.Read()); handle::Disconnect(); M.Change(8); post(M.Read()); post(handle.Connected);
  )", {{"./observable.clp", R"(
    observable number value = 0;
    def Receiver { number Seen = 0; void Changed(number next) { @this.Seen = next; } }
    auto receiver = Receiver {};
    export connection Listen() { return value.OnChange(receiver.Changed); }
    export void Change(number next) { value = next; }
    export number Read() { return receiver.Seen; }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "7\n7\nfalse\n");
}

TEST_CASE("typed signals retain callbacks and fire in registration order", "[event-subscription]") {
  const auto result = run(R"(
    signal<number> event;
    number total = 0;
    auto first = event::Connect(func(value) -> void { total += value; post("first"); });
    auto second = event::Connect(func(number value) -> void { post(total); });
    event::Fire(2); event::Fire(3);
    post(first.Connected); first::Disconnect(); first::Disconnect(); post(first.Connected);
    event::Fire(5); second::Disconnect(); event::Fire(10); post(total);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "first\n2\nfirst\n5\ntrue\nfalse\n5\n5\n");
}

TEST_CASE("managed subscriptions use the event internal caretaker and clear idempotently", "[event-subscription]") {
  const auto result = run(R"(
    signal<number> event;
    auto regular = event::Connect(func(value) -> void { post("regular"); });
    auto managed = event~>Connect(func(value) -> void { post("managed"); });
    event::Fire(1); event::Clear(); event::Clear();
    post(regular.Connected); post(managed.Connected); event::Fire(2);
    event~>Connect(func(value) -> void { post("reused"); }); event::Fire(3);
    regular::Disconnect(); event::Clear(); event::Fire(4);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "regular\nmanaged\ntrue\nfalse\nregular\nregular\nreused\n");
}

TEST_CASE("once and mutation during fire preserve a stable subscription snapshot", "[event-subscription]") {
  const auto result = run(R"(
    signal<number> event;
    auto once = event::Once(func(value) -> void { post("once"); if (value == 1) { event::Fire(2); } });
    event::Connect(func(value) -> void { post(value); });
    event::Fire(1); post(once.Connected); event::Fire(3);
    signal<number> other;
    number added = 0;
    other::Connect(func(value) -> void {
      post("early");
      if (added == 0) { added = 1; other::Connect(func(value) -> void { post("late"); }); }
    });
    other::Fire(1); other::Fire(2);
    signal<number> cleanup;
    cleanup~>Connect(func(value) -> void { post("clear"); cleanup::Clear(); });
    cleanup~>Connect(func(value) -> void { post("skipped"); }); cleanup::Fire(1);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "once\n2\n1\nfalse\n3\nearly\nearly\nlate\nclear\n");
}

TEST_CASE("signal callback failures stop dispatch and remain catchable", "[event-subscription]") {
  const auto result = run(R"(
    signal<number> event;
    event::Connect(func(value) -> void { post("before"); });
    auto failed = event::Connect(func(value) -> void { report("handler failure"); });
    event::Connect(func(value) -> void { post("after"); });
    try { event::Fire(1); } catch (error) { post(error); }
    failed::Disconnect(); event::Fire(2);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "before\nhandler failure\nbefore\nafter\n");
}

TEST_CASE("signal payloads callbacks and managed operations are statically checked", "[event-subscription]") {
  for (const auto& source : {
    "signal<number> event; event::Fire(\"bad\");",
    "signal<number> event; event::Fire();",
    "signal<number> event; event::Connect(func(string value) -> void {});",
    "signal<number> event; event::Connect(func(value) -> number { return value; });",
    "signal<number> event; event~>Connect(async func(value) -> void {});",
    "signal<number> event; event~>Connect(func(value) -> void {}, 1);",
    "signal<number> event; signal<string> other = event;",
    "signal<number> event; signal<float> other = event;",
    "signal<Missing> event;",
    "signal<void> event;",
    "signal<number> event; event::Wait();",
    "signal<number> event; auto connection = event::Connect(func(value) -> void {}); number value = connection;"
  }) { INFO(source); CHECK_FALSE(run(source).compiled); }
}

TEST_CASE("typed signals preserve generic module identities float payloads and bound handlers", "[event-subscription]") {
  const auto result = run(R"(
    link "./events.clp" as E;
    auto event = E.Get();
    auto connection = event~>Connect(func(player) -> void { post(player.Health); });
    E.Emit(42); event::Clear(); E.Emit(23); post(connection.Connected);
    template <typename T> signal<T> Make() { signal<T> event; return event; }
    auto text = Make<string>(); text::Connect(func(value) -> void { post(value); }); text::Fire("typed");
    signal<float> decimals; decimals::Connect(func(value) -> void { post(value / 2); }); decimals::Fire(3);
    def Receiver { number Seen = 0; void Changed(number value) { @this.Seen = value; } }
    auto receiver = Receiver {}; signal<number> values;
    values~>Connect(receiver.Changed); values::Fire(7); post(receiver.Seen);
    signal<> empty; empty::Once(func() -> void { post("empty"); }); empty::Fire(); empty::Fire();
    def Owner<T> { signal<T> Event; }
    auto owner = Owner<string> { Event = signal<string>() }; owner.Event~>Connect(func(value) -> void { post(value); }); owner.Event::Fire("field");
    def Notifications<T> = signal<T>; Notifications<number> alias = signal<number>();
    alias~>Connect(func(value) -> void { post(value); }); alias::Fire(19); alias::Clear();
  )", {{"./events.clp", R"(
    export def Player { number Health; }
    signal<Player> state;
    export signal<Player> Get() { return state; }
    export void Emit(number value) { state::Fire(Player { Health = value }); }
  )"}});
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\nfalse\ntyped\n1.5\n7\nempty\nfield\n19\n");
}

TEST_CASE("signal connections survive returned closures and synchronized workers", "[event-subscription]") {
  const auto result = run(R"(
    signal<number> event; number total = 0; auto mutexHandle = mutex();
    connection Register(number initial) {
      number captured = initial;
      return event~>Connect(func(value) -> void { captured += value; total = captured; });
    }
    auto connection = Register(10);
    number Emit(number value) { lock(mutexHandle); event::Fire(value); unlock(mutexHandle); return value; }
    auto first = thread Emit(2); post(join(first)); post(total);
    auto second = thread Emit(3); post(join(second)); post(total);
    event::Clear(); post(connection.Connected);
  )");
  INFO(result.error); INFO(result.detail); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "2\n12\n3\n15\nfalse\n");
}

TEST_CASE("type first expressions check literal unions and preserve single evaluation", "[type-ascription]") {
  const auto result = run(R"(
    def Outcome = "ongoing" | "team_a" | "team_b";
    def Battle { Outcome Result; }
    auto battle = Battle { Result = "ongoing" };
    if (battle.Result != (Outcome "ongoing")) { report("wrong comparison"); }
    auto result = (Outcome "team_a"); post(result);
    number calls = 0;
    number Next() { calls += 1; return calls; }
    post((number Next())); post(calls);
    post((float 7) / 2);
    post((number -3));
    number Read(number value) { return value; }
    post((Read (4)));
    number n = 9; post((n - 2));
    def Count = number;
    number Shadow() { number Count = 9; return (Count - 2); }
    post(Shadow());
    def Point { number X; }
    post((Point { X = 8 }).X);
  )");
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  CHECK(result.output == "team_a\n1\n1\n3.5\n-3\n4\n7\n7\n8\n");
}

TEST_CASE("type first expressions preserve generic containers callbacks and module identity", "[type-ascription]") {
  const auto result = run(R"(
    link "./types.clp" as Types;
    def Box<T> { T Value; }
    template <typename T> T Keep(T value) { return (T value); }
    def Callback = (number) -> number;
    auto increment = (Callback func(value) -> number { return value + 1; });
    auto values = (array<number> { 3, 4 }); post(values[1]);
    post((Box<number> Box<number> { Value = 7 }).Value);
    post((Types.Outcome "ongoing"));
    post((Types.Outcome ("ongoing")));
    auto empty = (Types.Numbers {}); post(len(empty));
    optional<number> absent = (optional<number> null); post(absent == null);
    post(Keep("generic")); post(increment(4));
  )", {{"./types.clp", "export def Outcome = \"ongoing\" | \"done\"; export def Numbers = array<number>;"}});
  REQUIRE(result.compiled);
  REQUIRE(result.ran);
  CHECK(result.output == "4\n7\nongoing\nongoing\n0\ntrue\ngeneric\n5\n");
}

TEST_CASE("return leaves protected scopes without retaining a dead catch", "[try-lifetime]") {
  const auto result = run(R"(
    number Read(number value) {
      try { try { return value; } catch { return -1; } } catch { return -2; }
    }
    void Done() { try { return; } catch { post("dead void catch"); } }
    number Divide(number value) { try { return 10 / value; } catch { return 99; } }
    number Recurse(number value) { try { if (value == 0) { return 7; } return Recurse(value - 1); } catch { return -3; } }
    try {
      post(Read(42)); Done(); post(Divide(0)); post(Recurse(4));
      report("original failure");
    } catch (error) { post(error); }
    post("finished");
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "42\n99\n7\noriginal failure\nfinished\n");
}

TEST_CASE("break and continue leave only the protected scopes they exit", "[try-lifetime]") {
  const auto result = run(R"(
    number stale = 0;
    try {
      for (number i = 0; i < 3; i++) {
        try { if (i < 2) { continue; } break; } catch { stale++; }
      }
      switch (1) { case 1: try { break; } catch { stale++; } default: post("wrong switch"); }
      try {
        while (true) { try { break; } catch { stale++; } }
        report("retained outer catch");
      } catch (error) { post(error); }
      report("top catch");
    } catch (error) { post(error); }
    post(stale);
  )");
  INFO(result.error); REQUIRE(result.compiled); REQUIRE(result.ran);
  CHECK(result.output == "retained outer catch\ntop catch\n0\n");
}

TEST_CASE("portable JSON decimal parsing preserves ranges and negative zero", "[typed-json][portable-decimal]") {
  for (const auto* spelling : {"0.0", "-0.0", "1.5", "1e-300", "4.9406564584124654e-324", "1.7976931348623157e308"}) {
    INFO(spelling);
    double standard = 0, portable = 0;
    REQUIRE(clpp::stdlib::detail::parse_decimal(std::string_view(spelling), standard));
    REQUIRE(clpp::stdlib::detail::parse_decimal_classic(std::string_view(spelling), portable));
    CHECK(standard == portable);
    CHECK(std::signbit(standard) == std::signbit(portable));
  }
  for (const auto* spelling : {"1e309", "1e-400", "1.5trailing", "nan", "inf", ""}) {
    INFO(spelling);
    double value = 0;
    CHECK_FALSE(clpp::stdlib::detail::parse_decimal_classic(std::string_view(spelling), value));
  }
}

TEST_CASE("type first expressions cannot force incompatible or unchecked values", "[type-ascription]") {
  for (const auto* source : {
    "def Outcome = \"ongoing\" | \"done\"; post((Outcome \"invalid\"));",
    "def Outcome = \"ongoing\" | \"done\"; string value=\"ongoing\"; post((Outcome value));",
    "post((number 1.5));", "post((number true));", "post((Unknown \"value\"));",
    "auto value=(array<number> { \"bad\" });",
    "any Dynamic() { return 1; } post((number Dynamic()));",
    "def A { number Value; } def B { number Value; } post((A B { Value=1 }));"
  }) {
    INFO(source);
    CHECK_FALSE(run(source).compiled);
  }
}
