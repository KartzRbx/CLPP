// What each reserved word of CL++ is, for the editor.
//
// The lexer reserves 77 words, but they are not all "keywords" to the person typing: `int` is a
// type, `post` is a function, `true` is a constant. Completion and hover read this table so each
// word gets the right icon, a one-line description and an example. A unit test checks that every
// word the lexer reserves is described here.

#include "clpp/ide.hpp"

#include <array>

namespace clpp::ide {

namespace {

constexpr std::array kKeywordDocs = {
    // --- types ------------------------------------------------------------------------------
    KeywordDoc{"int", Kind::Type, "primitive type · integer",
               "Whole number. Dividing two `int`s drops the fraction.\n\n```clpp\nint lives = 3;\npost(7 / 2);  << 3\n```"},
    KeywordDoc{"float", Kind::Type, "primitive type · decimal number",
               "64-bit decimal number. An `int` is accepted wherever a `float` is expected.\n\n```clpp\nfloat speed = 2.5;\n```"},
    KeywordDoc{"double", Kind::Type, "primitive type · decimal number (same as float)",
               "Synonym of `float`, for people coming from C/C++.\n\n```clpp\ndouble ratio = 0.75;\n```"},
    KeywordDoc{"bool", Kind::Type, "primitive type · true / false",
               "Logical value. Comparisons (`hp > 0`) produce a `bool`.\n\n```clpp\nbool alive = hp > 0;\n```"},
    KeywordDoc{"string", Kind::Type, "primitive type · UTF-8 text",
               "Immutable UTF-8 text. `len`, indexes and `for … in` count characters, not bytes.\n\n```clpp\nstring name = \"Ada\";\npost(name .: \"!\");\n```"},
    KeywordDoc{"void", Kind::Type, "type · no value",
               "Marks a function that returns nothing.\n\n```clpp\nfunc log(string m) -> void { post(m); }\n```"},
    KeywordDoc{"auto", Kind::Type, "type inferred by the compiler",
               "Declares a mutable variable whose type the compiler infers.\n\n```clpp\nauto base = 3;\n```"},
    KeywordDoc{"buffer", Kind::Type, "built-in type · block of bytes",
               "Fixed-size block of bytes, for network packets and binary files.\n\n```clpp\nbuffer b = buffer::create(64);\nbuffer::write_string(b, 0, \"CL++\");\npost(buffer::size(b));\n```"},
    KeywordDoc{"task", Kind::Type, "type · asynchronous task",
               "What `spawn` returns: a running task. `await` gives its value. `task.defer(f)` schedules `f`.\n\n```clpp\ntask t = spawn load(\"map\");\npost(await t);\n```"},
    KeywordDoc{"Vector2", Kind::Struct, "built-in type · 2D vector (x, y)",
               "Value type with `x` and `y`. Add with `+`, scale with `*`.\n\n```clpp\nVector2 pos = Vector2(3, 4);\npos.x += 1;\n```"},
    KeywordDoc{"Vector3", Kind::Struct, "built-in type · 3D vector (x, y, z)",
               "Value type with `x`, `y`, `z`. Also the RGB color (0–1) used by Axiom.\n\n```clpp\nVector3 velocity = Vector3(0, 9.8, 0) * 0.5;\n```"},
    KeywordDoc{"Vector4", Kind::Struct, "built-in type · 4D vector (x, y, z, w)",
               "Value type with `x`, `y`, `z`, `w`.\n\n```clpp\nVector4 rect = Vector4(0, 0, 320, 200);\n```"},

    // --- constants ----------------------------------------------------------------------------
    KeywordDoc{"true", Kind::Constant, "constant · true", "The `bool` value true."},
    KeywordDoc{"false", Kind::Constant, "constant · false", "The `bool` value false."},
    KeywordDoc{"null", Kind::Constant, "constant · no value",
               "Absence of a value. Prefer `Option<T>` (`Some`/`None`) for values that may be missing."},
    KeywordDoc{"endl", Kind::Constant, "constant · end of line", "A line break, for people coming from C++.\n\n```clpp\npost(endl);\n```"},

    // --- built-in functions ---------------------------------------------------------------------
    KeywordDoc{"post", Kind::Function, "post(value) · prints a line",
               "Writes the value to standard output, followed by a newline. Accepts any type.\n\n```clpp\npost(`${name}: ${hp} hp`);\n```"},
    KeywordDoc{"cout", Kind::Function, "cout(value) · prints (same as post)", "Synonym of `post`, for people coming from C++.\n\n```clpp\ncout(\"hello\");\n```"},
    KeywordDoc{"warn", Kind::Function, "warn(value) · warning on stderr",
               "Writes `warning: value` to the error stream without stopping the program.\n\n```clpp\nif (fps < 30) { warn(\"low fps: \" .: fps); }\n```"},
    KeywordDoc{"report", Kind::Function, "report(value) · raises an error",
               "Stops execution with the value as the message; catchable with `try`/`catch`.\n\n```clpp\nif (id < 1) { report(\"invalid level\"); }\n```"},
    KeywordDoc{"pcall", Kind::Function, "pcall(expression) -> 1 | 0",
               "Evaluates the expression and returns `1` if it worked or `0` if it failed, without stopping.\n\n```clpp\npost(pcall(10 / 0));  << 0\n```"},
    KeywordDoc{"join", Kind::Function, "join(thread) · waits for a thread",
               "Waits for a thread started with `thread f(...)` and returns its result.\n\n```clpp\nlet t = thread simulate(1000);\npost(join(t));\n```"},
    KeywordDoc{"move", Kind::Function, "move(variable) · hands over without copying",
               "Returns the variable's value and leaves the variable empty, without a copy.\n\n```clpp\nlet taken = move(text);\n```"},

    // --- declarations -------------------------------------------------------------------------
    KeywordDoc{"let", Kind::Keyword, "declaration · variable",
               "`let` declares an immutable variable; `let mut` a mutable one. The type is inferred or written after `:`.\n\n```clpp\nlet gravity = 9.8;\nlet mut score: int = 0;\n```"},
    KeywordDoc{"mut", Kind::Keyword, "modifier · mutable variable",
               "Used with `let` to allow reassignment.\n\n```clpp\nlet mut speed = 0;\nspeed += 5;\n```"},
    KeywordDoc{"const", Kind::Keyword, "declaration · constant",
               "A fixed value that cannot be reassigned. Top-level constants are visible inside functions and exported by modules.\n\n```clpp\nconst MAX_HP = 100;\n```"},
    KeywordDoc{"constexpr", Kind::Keyword, "declaration · compile-time constant",
               "Like `const`, but the value must be computable at compile time from literals.\n\n```clpp\nconstexpr SECONDS_PER_HOUR = 60 * 60;\n```"},
    KeywordDoc{"func", Kind::Keyword, "declaration · function",
               "Declares a function (or a method, inside a `struct`). Parameter and return types are optional.\n\n```clpp\nfunc add(int a, int b) -> int { return a + b; }\n```"},
    KeywordDoc{"struct", Kind::Keyword, "declaration · type with fields and methods",
               "Groups fields and methods. Inherits with `:`, can be generic.\n\n```clpp\nstruct Player : Entity {\n  int hp;\n  func damage(int amount) { @hp -= amount; }\n}\n```"},
    KeywordDoc{"enum", Kind::Keyword, "declaration · closed set of cases",
               "Named cases numbered from 0; cases can carry data. `match` requires every case.\n\n```clpp\nenum State { Idle, Running }\nenum Event { Damage(int), Say(string) }\n```"},
    KeywordDoc{"variant", Kind::Keyword, "declaration · one value among several types",
               "Holds a value of one of the listed types.\n\n```clpp\nvariant Value { int, string }\nlet v = Value(\"seven\");\n```"},
    KeywordDoc{"type", Kind::Keyword, "declaration · type name / union",
               "Names a type. Unions of literal strings are checked under `<<!strict`.\n\n```clpp\ntype State = \"Idle\" | \"Running\";\n```"},
    KeywordDoc{"namespace", Kind::Keyword, "declaration · groups functions",
               "Groups functions under a name.\n\n```clpp\nnamespace Physics {\n  func gravity() -> float { return 9.8; }\n}\npost(Physics.gravity());\n```"},
    KeywordDoc{"link", Kind::Keyword, "module · imports a file or library",
               "Imports a module (no header files). `@clpp.*` is the standard library.\n\n```clpp\nlink @clpp.axiom as Axiom;\nlink \"./combat.clp\" as Combat;\n```"},
    KeywordDoc{"import", Kind::Keyword, "module · synonym of link", "Same as `link`.\n\n```clpp\nimport @clpp.math as Math;\n```"},
    KeywordDoc{"from", Kind::Keyword, "module · synonym of link", "Same as `link`.\n\n```clpp\nfrom @clpp.text as Text;\n```"},
    KeywordDoc{"as", Kind::Keyword, "module · name of the imported module",
               "The name the module is used by.\n\n```clpp\nlink @clpp.window as Window;\n```"},
    KeywordDoc{"using", Kind::Keyword, "module · brings a name into scope",
               "Lets you call a module function without its prefix.\n\n```clpp\nimport @clpp.math as Math;\nusing Math.abs;\npost(abs(-7));\n```"},
    KeywordDoc{"extern", Kind::Keyword, "declaration · native function of the host",
               "Declares a function provided by the program that embeds CL++ (the game engine).\n\n```clpp\nextern func strlen(string s);\n```"},
    KeywordDoc{"static", Kind::Keyword, "modifier · static function",
               "Accepted for C++ familiarity; `static func` behaves like `func`."},
    KeywordDoc{"signal", Kind::Keyword, "declaration · event",
               "A named event. `~>` connects functions; calling the signal fires all of them.\n\n```clpp\nsignal died;\ndied ~> showGameOver;\ndied(1200);\n```"},
    KeywordDoc{"observable", Kind::Keyword, "declaration · variable that reports changes",
               "Calls the functions registered with `.OnChange` after every assignment.\n\n```clpp\nobservable int coins = 10;\ncoins.OnChange(func (int v) { post(v); });\ncoins = 25;\n```"},
    KeywordDoc{"atomic", Kind::Keyword, "declaration · counter shared between threads",
               "A value threads can share. Use `fetch_add` and `atomic_load`.\n\n```clpp\natomic hits = 0;\nfetch_add(hits, 1);\n```"},
    KeywordDoc{"operator", Kind::Keyword, "declaration · operator for a type",
               "Defines an operator for your structs.\n\n```clpp\nfunc operator+(Money a, Money b) { return Money(a.cents + b.cents); }\n```"},
    KeywordDoc{"decltype", Kind::Keyword, "type of another variable",
               "Uses the type of an existing variable.\n\n```clpp\ndecltype(base) other = 4;\n```"},
    KeywordDoc{"new", Kind::Keyword, "construction (optional)",
               "Synonym of calling the constructor, for people coming from C++/C#.\n\n```clpp\nPlayer p = new Player(\"Ada\", 100);\n```"},

    // --- modifiers ----------------------------------------------------------------------------
    KeywordDoc{"abstract", Kind::Keyword, "modifier · abstract struct/method",
               "An `abstract struct` can only be inherited; an `abstract func` must be implemented by children.\n\n```clpp\nabstract struct Shape { abstract func area() -> float {} }\n```"},
    KeywordDoc{"override", Kind::Keyword, "modifier · replaces a parent method",
               "States that the method replaces one of the parent (error if the parent has none).\n\n```clpp\noverride func area() -> float { return @side * @side; }\n```"},
    KeywordDoc{"final", Kind::Keyword, "modifier · cannot be inherited/overridden",
               "A `final struct` cannot be inherited; a `final func` cannot be overridden."},
    KeywordDoc{"private", Kind::Keyword, "modifier · only the struct itself can access",
               "Field or method reachable only from the struct's own methods.\n\n```clpp\nstruct Account { private int pin; }\n```"},
    KeywordDoc{"public", Kind::Keyword, "modifier · public access (default)", "Members are public by default; `public` makes it explicit."},
    KeywordDoc{"async", Kind::Keyword, "modifier · asynchronous function",
               "An `async func` can be started with `spawn` (returns a `task`) and waited for with `await`.\n\n```clpp\nasync func load(string name) { return name; }\n```"},

    // --- control flow -------------------------------------------------------------------------
    KeywordDoc{"if", Kind::Keyword, "control flow · condition",
               "Runs the block when the condition is true. Braces are required.\n\n```clpp\nif (hp <= 0) { post(\"game over\"); } else { post(\"alive\"); }\n```"},
    KeywordDoc{"else", Kind::Keyword, "control flow · otherwise", "Block that runs when the `if` condition is false."},
    KeywordDoc{"while", Kind::Keyword, "control flow · loop with a condition",
               "Repeats while the condition is true.\n\n```clpp\nwhile (Window.Frame()) { … }\n```"},
    KeywordDoc{"for", Kind::Keyword, "control flow · loop",
               "`for (let x in source)` walks numbers, lists, strings and dictionaries; the C style also exists.\n\n```clpp\nfor (let i in 3) { post(i); }\nfor (let mut i = 0; i < 10; i += 2) { post(i); }\n```"},
    KeywordDoc{"in", Kind::Keyword, "control flow · part of for … in", "`for (let item in items) { … }`"},
    KeywordDoc{"break", Kind::Keyword, "control flow · leaves the loop", "Leaves the current `while`, `for` or `switch`."},
    KeywordDoc{"continue", Kind::Keyword, "control flow · next iteration", "Jumps to the next iteration of the loop."},
    KeywordDoc{"return", Kind::Keyword, "control flow · returns from the function", "Leaves the function, optionally with a value.\n\n```clpp\nreturn a + b;\n```"},
    KeywordDoc{"match", Kind::Keyword, "control flow · pattern matching",
               "Tries patterns in order; with enums, every case is required.\n\n```clpp\nmatch (state) {\n  Idle ~> post(\"idle\");\n  _ ~> post(\"other\");\n}\n```"},
    KeywordDoc{"guard", Kind::Keyword, "control flow · extra condition of a match arm",
               "```clpp\nmatch (score) {\n  _ guard score >= 80 ~> post(\"great\");\n  _ ~> post(\"ok\");\n}\n```"},
    KeywordDoc{"switch", Kind::Keyword, "control flow · choice by value",
               "Compares with each `case`; there is no fall-through.\n\n```clpp\nswitch (key) {\n  case 1: post(\"jump\"); break;\n  default: post(\"idle\"); break;\n}\n```"},
    KeywordDoc{"case", Kind::Keyword, "control flow · case of a switch", "`case value: statements`"},
    KeywordDoc{"default", Kind::Keyword, "control flow · default case of a switch", "Runs when no `case` matches."},
    KeywordDoc{"try", Kind::Keyword, "errors · protects a block",
               "A runtime error inside the block jumps to `catch`.\n\n```clpp\ntry { post(xs[9]); } catch (e) { post(\"failed: \" .: e); }\n```"},
    KeywordDoc{"catch", Kind::Keyword, "errors · handles the error of try", "Receives the error message (the name is optional)."},
    KeywordDoc{"where", Kind::Keyword, "generics · type constraint",
               "Restricts the types a generic accepts.\n\n```clpp\nfunc onlyInt<T>(T v) where T: int { return v; }\n```"},

    // --- concurrency --------------------------------------------------------------------------
    KeywordDoc{"spawn", Kind::Keyword, "concurrency · starts a task",
               "Starts an `async` function and returns a `task` right away.\n\n```clpp\ntask t = spawn load(\"map\");\n```"},
    KeywordDoc{"await", Kind::Keyword, "concurrency · waits for a task", "Waits for the `task` and returns its result.\n\n```clpp\npost(await t);\n```"},
    KeywordDoc{"parallel", Kind::Keyword, "concurrency · several tasks at once",
               "Starts several `async` calls, waits for all of them and returns the list of results.\n\n```clpp\nlet r = parallel(cost(2), cost(5));\n```"},
    KeywordDoc{"thread", Kind::Keyword, "concurrency · native thread",
               "Runs the function on an OS thread; `join` waits for the result.\n\n```clpp\nlet t = thread simulate(1000);\npost(join(t));\n```"},

    // --- word operators -----------------------------------------------------------------------
    KeywordDoc{"and", Kind::Keyword, "logical operator · and", "True when both sides are true (same as `&&`)."},
    KeywordDoc{"or", Kind::Keyword, "logical operator · or", "True when either side is true (same as `||`)."},
    KeywordDoc{"not", Kind::Keyword, "logical operator · negation", "Inverts a logical value (same as `!`)."},
    KeywordDoc{"shl", Kind::Keyword, "bit operator · shift left", "`(1 shl 3) == 8`"},
    KeywordDoc{"shr", Kind::Keyword, "bit operator · shift right", "`(16 shr 2) == 4`"},
};

}  // namespace

const KeywordDoc* keyword_doc(const std::string_view word) {
  for (const KeywordDoc& entry : kKeywordDocs) {
    if (entry.name == word) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace clpp::ide
