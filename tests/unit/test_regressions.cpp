// Regression tests for the problems found in the CL++ rework review (see docs/CHANGES.md).
#include "clpp/clir.hpp"
#include "clpp/compiler.hpp"
#include "clpp/core/lexer/lexer.hpp"
#include "clpp/vm.hpp"
#include "core/compiler/analyze.hpp"

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("a line comment in a CRLF file does not swallow the program", "[regression][lexer]") {
  const Run result = run("<< comentario\r\npost(\"hello\");\r\npost(1);\r\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "hello\n1\n");
}

TEST_CASE("a lone CR also ends a line comment", "[regression][lexer]") {
  const Run result = run("<< old mac\rpost(2);\r");
  REQUIRE(result.output == "2\n");
}

TEST_CASE("int division truncates toward zero, float division does not", "[regression][semantics]") {
  const Run result = run("post(7 / 2);\npost(-7 / 2);\npost(7.0 / 2);\nlet a = 9;\nlet b = 4;\npost(a / b);\npost(a % b);\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n-3\n3.5\n2\n1\n");
}

TEST_CASE("whole numbers print exactly", "[regression][vm]") {
  const Run result = run("let mut s = 0;\nlet mut i = 0;\nwhile (i < 100000) { s += i; i += 1; }\npost(s * 1000);\npost(0.5);\n");
  REQUIRE(result.output == "4999950000000\n0.5\n");
}

TEST_CASE("deep recursion does not overflow at 256 calls", "[regression][vm]") {
  const Run result = run("func d(n) {\n  if (n == 0) { return 0; }\n  return 1 + d(n - 1);\n}\npost(d(50000));\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "50000\n");
}

TEST_CASE("modules export structs with inheritance, enums, functions and constants", "[regression][modules]") {
  const std::map<std::string, std::string> files = {
      {"./shapes.clp",
       "const MAX_HP = 100;\n"
       "struct Animal {\n  int legs;\n  func speak() { return self.legs * 10; }\n}\n"
       "struct Dog : Animal {\n  int bones;\n}\n"
       "enum Color { Red, Green, Blue }\n"
       "func area(w, h) { return w * h; }\n"
       "func twiceArea(w, h) {\n  let mut total = 0;\n  for (let mut i = 0; i < 2; i += 1) { total += area(w, h); }\n  return total;\n}\n"
       "func clampHp(hp) {\n  if (hp > MAX_HP) { return MAX_HP; }\n  return hp;\n}\n"}};
  const Run result = run(
      "link \"./shapes.clp\" as Shapes;\n"
      "Dog d = Shapes.Dog(4, 2);\n"
      "post(d.legs);\npost(d.speak());\n"
      "post(Shapes.twiceArea(3, 4));\n"
      "post(Shapes.Color.Green);\n"
      "post(Shapes.MAX_HP);\n"
      "post(Shapes.clampHp(250));\n",
      files);
  REQUIRE(result.compiled);
  REQUIRE(result.output == "4\n40\n24\n1\n100\n100\n");
}

TEST_CASE("an error inside a module is reported on the link line", "[regression][modules]") {
  const Run result = run("link \"./bad.clp\" as Bad;\npost(1);\n", {{"./bad.clp", "func f() {\n  return nope;\n}\n"}});
  REQUIRE_FALSE(result.compiled);
  REQUIRE(result.error.find("error in module ./bad.clp (2:10)") == 0);
}

TEST_CASE("a module cannot run top-level code", "[regression][modules]") {
  const Run result = run("link \"./m.clp\" as M;\n", {{"./m.clp", "post(1);\n"}});
  REQUIRE_FALSE(result.compiled);
  REQUIRE(result.error.find("a module can only declare") == 0);
}

TEST_CASE("top-level constants are visible in functions unless shadowed", "[regression][semantics]") {
  const Run result = run("const X = 1;\nfunc f(X) { return X; }\nfunc g() { let X = 5; return X; }\nfunc h() { return X + 1; }\n"
                         "post(f(7));\npost(g());\npost(h());\n");
  REQUIRE(result.output == "7\n5\n2\n");
}

TEST_CASE("diagnostics own their message after the analysis is gone", "[regression][api]") {
  clpp::CompileResult result = clpp::Compiler{}.compile("link \"./x.clp\" as X;\n", [](std::string_view) {
    return std::optional<std::string>("func f() { return nope; }\n");
  });
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.diagnostics.front().message.find("error in module") == 0);
}

TEST_CASE("the Luau backend keeps grouping, escapes strings and keeps float digits", "[regression][luau]") {
  const auto luau = [](const std::string& source) {
    const clpp::AnalysisResult analysis = clpp::analyze_program(source, {});
    return clpp::clir_to_luau(analysis).source;
  };
  REQUIRE(luau("post((1 + 2) * 3);\n") == "print((1 + 2) * 3)\n");
  REQUIRE(luau("post(10 - (3 - 2));\n") == "print(10 - (3 - 2))\n");
  REQUIRE(luau("post(\"say \\\"hi\\\"\");\n") == "print(\"say \\\"hi\\\"\")\n");
  REQUIRE(luau("post(3.14159265);\n") == "print(3.14159265)\n");
  REQUIRE(luau("post(7 / 2);\n") == "print((math.modf(7 / 2)))\n");
}

TEST_CASE("len counts elements, dictionary entries and UTF-8 characters", "[regression]") {
  const Run result = run(
      "post(len(list(4, 5, 6)));\n"
      "post(len(\"ação\"));\n"
      "dictionary<string, int> d = dictionary<string, int>(\"a\", 1, \"b\", 2);\n"
      "post(len(d));\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n4\n2\n");
  REQUIRE_FALSE(run("post(len(3));\n").compiled);
}

TEST_CASE("for-in walks lists, strings and dictionary keys; numbers still count", "[regression]") {
  const Run result = run(
      "for (let v in list(5, 6)) { post(v); }\n"
      "for (let c in \"hé\") { post(c); }\n"
      "dictionary<string, int> d = dictionary<string, int>(\"x\", 1);\n"
      "for (let k in d) { post(k); }\n"
      "for (let i in 2) { post(i); }\n"
      "for (let v in list(1, 2, 3)) { if (v == 2) { break; } post(v); }\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "5\n6\nh\né\nx\n0\n1\n1\n");
}

TEST_CASE("fields, nested fields, vector components and list elements can be assigned", "[regression]") {
  const Run result = run(
      "struct Stats { int hp; int mana; }\n"
      "struct Player { string name; Stats stats; Vector3 pos; }\n"
      "Player p = Player(\"Ada\", Stats(100, 50), Vector3(0, 0, 0));\n"
      "p.name = \"Bea\";\n"
      "p.stats.hp -= 30;\n"
      "p.pos.y = 4.5;\n"
      "post(p.name .: \" \" .: p.stats.hp .: \" \" .: p.pos.y);\n"
      "let mut xs = list(1, 2, 3);\n"
      "xs[1] = 20;\n"
      "xs[2] *= 3;\n"
      "post(xs[1] .: \" \" .: xs[2]);\n"
      "array<Stats> team = array<Stats>(Stats(1, 1), Stats(2, 2));\n"
      "team[1].hp = 99;\n"
      "post(team[1].hp);\n"
      "dictionary<string, int> bag = dictionary<string, int>(\"gold\", 1);\n"
      "bag[\"gold\"] += 9;\n"
      "bag[\"gem\"] = 2;\n"
      "post(bag[\"gold\"] .: \" \" .: bag[\"gem\"] .: \" \" .: len(bag));\n"
      "Player copy = p;\n"
      "copy.stats.hp = 1;\n"
      "post(p.stats.hp .: \" \" .: copy.stats.hp);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "Bea 70 4.5\n20 9\n99\n10 2 2\n70 1\n");
  REQUIRE_FALSE(run("struct S { int hp; }\nlet s = S(1);\ns.hp = 2;\n").compiled);
}

TEST_CASE("methods that change self update the variable they were called on", "[regression]") {
  const Run result = run(
      "struct Animal { int age; func birthday() { self.age += 1; } func years() -> int { return self.age; } }\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "  func fetch() -> int { self.bones += 1; self.birthday(); return self.bones; }\n"
      "}\n"
      "Dog d = Dog(3, 0);\n"
      "d.birthday();\n"
      "post(d.fetch());\n"
      "post(d.years());\n"
      "func hurt(Dog other) { other.age = 0; return other.age; }\n"
      "post(hurt(d));\n"
      "post(d.age);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "1\n5\n0\n5\n");
}

TEST_CASE("continue skips to the next iteration of every loop kind", "[regression]") {
  const Run result = run(
      "for (let i in 5) { if (i % 2 == 0) { continue; } post(i); }\n"
      "for (let v in list(1, 2, 3)) { if (v == 2) { continue; } post(v); }\n"
      "let mut n = 0;\n"
      "while (n < 4) { n = n + 1; if (n == 2) { continue; } post(n); }\n"
      "for (let mut j = 0; j < 3; j = j + 1) { if (j == 1) { continue; } post(j); }\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "1\n3\n1\n3\n1\n3\n4\n0\n2\n");
  REQUIRE_FALSE(run("continue;\n").compiled);
}

TEST_CASE("post shows lists, dictionaries, structs and vectors as written; any value joins as text", "[regression]") {
  const Run result = run(
      "struct P { string n; int hp; }\n"
      "post(list(1, 2, list(3)));\n"
      "post(list(\"a\", \"b\"));\n"
      "post(P(\"a\", 2));\n"
      "dictionary<string, int> d = dictionary<string, int>(\"g\", 1);\n"
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
      "let a = 1;\nlet b = 2;\npost(a | b);\npost(a & 3);\npost(a ^ b);\npost(a shl b);\npost(8 shr a);\npost(~a);\n"
      "let x = 6;\nlet y = x | 1;\npost(y);\n");
  REQUIRE(result.ran);
  REQUIRE(result.output == "3\n1\n3\n4\n4\n-2\n7\n");
}

TEST_CASE("operator+ applies to constructor calls and fields; distinct text literals compare", "[regression]") {
  const Run result = run(
      "struct Money { int cents; }\n"
      "func operator+(Money a, Money b) { return Money(a.cents + b.cents); }\n"
      "Money total = Money(150) + Money(275);\n"
      "post(total.cents);\n"
      "post(\"abc\" != \"abd\");\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "425\ntrue\n");
}

TEST_CASE("missing return is checked on every path; -> void is a return type", "[regression]") {
  REQUIRE_FALSE(run("func sign(int n) -> int {\n  if (n > 0) { return 1; }\n}\n").compiled);
  const Run ok = run(
      "func sign(int n) -> int {\n  if (n > 0) { return 1; } else { return 0; }\n}\n"
      "func forever() -> int { while (true) { return 2; } }\n"
      "func fail() -> int { report(\"no\"); }\n"
      "func log(string m) -> void { post(m); }\n"
      "post(sign(5));\npost(forever());\nlog(\"ok\");\n");
  INFO(ok.error);
  REQUIRE(ok.ran);
  REQUIRE(ok.output == "1\n2\nok\n");
}

TEST_CASE("@field inside a method is self.field; constructors take named arguments", "[regression]") {
  const Run result = run(
      "struct P {\n  string name;\n  int hp;\n"
      "  func hit() { @hp = @hp - 10; }\n"
      "  func show() { post(@this.name .: \" \" .: @hp); }\n}\n"
      "P p = P(hp: 50, name: \"Ada\");\n"
      "p.hit();\np.show();\n@coins = 3;\npost(@coins);\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "Ada 40\n3\n");
}

TEST_CASE("== and != compare enums, bools, structs, lists and vectors by value", "[regression]") {
  const Run result = run(
      "enum Direction { North, East }\nlet facing = Direction.East;\npost(facing == Direction.East);\n"
      "let alive = true;\npost(alive == true);\n"
      "struct P { int x; }\nP p = P(1);\nP q = P(2);\npost(p == q);\npost(p != q);\n"
      "post(list(1, 2) == list(1, 2));\npost(Vector3(1, 2, 3) == Vector3(1, 2, 3));\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "true\ntrue\nfalse\ntrue\ntrue\ntrue\n");
}

TEST_CASE("push, pop, insert and remove change the list variable; find works on any value", "[regression]") {
  const Run result = run(
      "let mut xs = list();\npush(xs, 10);\npush(xs, 20);\npush(xs, 30);\npost(pop(xs));\n"
      "insert(xs, 0, 5);\npost(remove(xs, 1));\npost(xs);\n"
      "dictionary<string, int> inv = dictionary<string, int>(\"gold\", 3, \"gem\", 1);\npost(remove(inv, \"gold\"));\npost(len(inv));\n"
      "let mut names = list(\"ana\", \"bia\");\npost(find(names, \"bia\"));\n"
      "func fill() { let mut out = list(); for (let i in 3) { push(out, i * i); } return out; }\npost(fill());\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "30\n10\n[5, 20]\n3\n1\n1\n[0, 1, 4]\n");
  REQUIRE_FALSE(run("let fixed = list(1);\npush(fixed, 2);\n").compiled);
}

TEST_CASE("@clpp.text: split, joinAll, replace, startsWith, endsWith, repeat, toNumber, indexOf", "[regression]") {
  const Run result = run(
      "link @clpp.text as Text;\n"
      "let parts = Text.split(\"a,b,c\", \",\");\npost(parts);\npost(Text.joinAll(parts, \"|\"));\n"
      "post(Text.replace(\"1-2-3\", \"-\", \"+\"));\npost(Text.startsWith(\"CL++\", \"CL\"));\n"
      "post(Text.endsWith(\"save.dat\", \".json\"));\npost(Text.repeat(\"ab\", 3));\n"
      "post(Text.toNumber(\" 42.5 \") + 1);\npost(Text.indexOf(\"ação!\", \"!\"));\n"
      "try { post(Text.toNumber(\"x1\")); } catch (e) { post(e); }\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output ==
          "[\"a\", \"b\", \"c\"]\na|b|c\n1+2+3\ntrue\nfalse\nababab\n43.5\n4\nnot a number: \"x1\"\n");
}

TEST_CASE("for-in over array<T> types the item, so its fields resolve", "[regression]") {
  const Run result = run(
      "struct Enemy { string name; int hp; }\n"
      "array<Enemy> wave = array<Enemy>(Enemy(\"slime\", 10), Enemy(\"orc\", 40));\n"
      "for (let e in wave) { post(e.name .: \" \" .: e.hp); }\n");
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "slime 10\norc 40\n");
}

TEST_CASE("link without `as` names the module after its last name; module types work qualified", "[regression]") {
  const Run result = run(
      "link \"./combat.clp\";\nlink @clpp.axiom;\n"
      "func heal(Combat.Fighter f) -> Combat.Fighter { f.hp += 5; return f; }\n"
      "Combat.Fighter a = Combat.Fighter(\"Ada\", Combat.MAX_HP);\n"
      "post(heal(a).hp);\npost(Axiom.Clamp(5, 0, 3));\n",
      {{"./combat.clp", "const MAX_HP = 100;\nstruct Fighter { string name; int hp; }\n"}});
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "105\n3\n");
}

TEST_CASE("a module's own links resolve inside its functions; two paths to one module share its types", "[regression]") {
  const std::map<std::string, std::string> files = {
      {"./combat.clp", "const MAX_HP = 100;\nstruct Fighter { string name; int hp; func alive() -> bool { return self.hp > 0; } }\n"
                       "func hit(Fighter f, int dmg) -> Fighter { f.hp -= dmg; return f; }\n"},
      {"./d.clp", "link \"./combat.clp\" as C2;\nfunc twice() { return C2.MAX_HP * 2; }\n"
                  "func make() -> Fighter { return C2.Fighter(\"z\", C2.MAX_HP); }\n"},
  };
  const Run result = run(
      "link \"./combat.clp\" as Combat;\nlink \"./d.clp\" as D;\n"
      "post(D.twice() + Combat.MAX_HP);\n"
      "Fighter f = D.make();\npost(Combat.hit(f, 1).hp);\npost(f.alive());\n",
      files);
  INFO(result.error);
  REQUIRE(result.ran);
  REQUIRE(result.output == "300\n99\ntrue\n");
}

TEST_CASE("a signal can connect a module function", "[regression]") {
  const Run result = run("link \"./hud.clp\" as Hud;\nsignal damaged;\ndamaged ~> Hud.onDamage;\ndamaged(15);\n",
                         {{"./hud.clp", "func onDamage(int amount) { post(\"HUD: -\" .: amount); }\n"}});
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
