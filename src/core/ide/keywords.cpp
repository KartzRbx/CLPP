// What each reserved word of CL++ is, for the editor.
//
// The lexer also reserves obsolete spellings for diagnostics, but they are not all "keywords" to the person typing: `number` is a
// type, `post` is a function, `true` is a constant. Completion and hover read this table so each
// word gets the right icon, a one-line description and an example. A unit test checks that every
// supported keyword has a grammar classification; rejected legacy spellings are excluded.

#include "clpp/ide.hpp"

#include <array>

namespace clpp::ide {

namespace {

constexpr std::array kKeywordDocs = {
    // --- types ------------------------------------------------------------------------------
    KeywordDoc{"number", Kind::Type, "primitive type · integer",
               "Whole number. Dividing two `number`s drops the fraction.\n\n```clpp\nnumber lives = 3;\npost(7 / 2);  << 3\n```"},
    KeywordDoc{"float", Kind::Type, "primitive type · decimal number",
               "64-bit decimal number. A `number` is accepted wherever a `float` is expected.\n\n```clpp\nfloat speed = 2.5;\n```"},
    KeywordDoc{"bool", Kind::Type, "primitive type · true / false",
               "Logical value. Comparisons (`hp > 0`) produce a `bool`.\n\n```clpp\nbool alive = hp > 0;\n```"},
    KeywordDoc{"string", Kind::Type, "primitive type · UTF-8 text",
               "Immutable UTF-8 text. `len`, indexes and `for … in` count characters, not bytes.\n\n```clpp\nstring name = \"Ada\";\npost(name .: \"!\");\n```"},
    KeywordDoc{"void", Kind::Type, "type · no value",
               "Marks a function that returns nothing.\n\n```clpp\nvoid Print(string m) { post(m); }\n```"},
    KeywordDoc{"buffer", Kind::Type, "built-in type · block of bytes",
               "Fixed-size block of bytes, for network packets and binary files.\n\n```clpp\nbuffer b = buffer::create(64);\nbuffer::write_string(b, 0, \"CL++\");\npost(buffer::size(b));\n```"},
    KeywordDoc{"task", Kind::Type, "type · asynchronous task",
               "What `spawn` returns: a running task. `await` gives its value. Tasks retain their result type.\n\n```clpp\ntask<string> t = spawn load(\"map\");\npost(await t);\n```"},
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
               "Absence of a value, distinct from zero. Use optional<T> and check against null before using the contained value."},

    // --- built-in functions ---------------------------------------------------------------------
    KeywordDoc{"post", Kind::Function, "post(value) · prints a line",
               "Writes the value to standard output, followed by a newline. Accepts any type.\n\n```clpp\npost(`${name}: ${hp} hp`);\n```"},
    KeywordDoc{"warn", Kind::Function, "warn(value) · warning on stderr",
               "Writes `warning: value` to the error stream without stopping the program.\n\n```clpp\nif (fps < 30) { warn(\"low fps: \" .: fps); }\n```"},
    KeywordDoc{"report", Kind::Function, "report(value) · raises an error",
               "Stops execution with the value as the message; catchable with `try`/`catch`.\n\n```clpp\nif (id < 1) { report(\"invalid level\"); }\n```"},
    KeywordDoc{"pcall", Kind::Function, "pcall(expression) -> 1 | 0",
               "Evaluates the expression and returns `1` if it worked or `0` if it failed, without stopping.\n\n```clpp\npost(pcall(10 / 0));  << 0\n```"},
    KeywordDoc{"join", Kind::Function, "join(thread) · waits for a thread",
               "Waits for a thread started with `thread f(...)` and returns its result.\n\n```clpp\nconst auto t = thread simulate(1000);\npost(join(t));\n```"},
    KeywordDoc{"move", Kind::Function, "move(variable) · hands over without copying",
               "Returns the variable's value and leaves the variable empty, without a copy.\n\n```clpp\nconst auto taken = move(text);\n```"},

    // --- declarations -------------------------------------------------------------------------
    KeywordDoc{"auto", Kind::Keyword, "declaration � inferred variable",
               "Infers a mutable variable's type from its initializer. Use `const auto` for an inferred constant.\n\n```clpp\nauto score = 0;\nscore += 5;\nconst auto limit = 100;\n```"},
    KeywordDoc{"export", Kind::Keyword, "module declaration",
               "Marks a module declaration as public. Declarations without export are internal to the module.\n\n```clpp\nexport number Add(number a, number b) { return a + b; }\nexport const number LIMIT = 100;\n```"},
    KeywordDoc{"const", Kind::Keyword, "declaration · constant",
               "A fixed value that cannot be reassigned. Top-level constants are visible inside functions. Use export to expose them from a module.\n\n```clpp\nconst number MAX_HP = 100;\n```"},
    KeywordDoc{"constexpr", Kind::Keyword, "declaration · compile-time constant",
               "Like `const`, with an explicit type or `auto`, but the value must be a literal or numeric arithmetic computable at compile time.\n\n```clpp\nconstexpr number SECONDS_PER_HOUR = 60 * 60;\n```"},
    KeywordDoc{"func", Kind::Keyword, "expression · lambda",
               "Creates an anonymous callback. The result type is explicit; parameter types can come from an unambiguous expected signature. Named functions put the result type before their name.\n\n```clpp\ndef Transform = (number) -> number;\nTransform addOne = func(value) -> number { return value + 1; };\n```"},
    KeywordDoc{"enum", Kind::Keyword, "declaration: closed set of scoped cases",
               "Declares a nominal enum type. Use :: to select a case; match requires every case or a wildcard.\n\n```clpp\nenum class State { Idle, Running }\nState state = State::Idle;\n```"},
    KeywordDoc{"class", Kind::Keyword, "enum modifier: scoped cases",
               "Used after enum. Cases belong to their enum type and are accessed with ::."},
    KeywordDoc{"link", Kind::Keyword, "module · imports a file or library",
               "Imports a module (no header files). `@clpp.*` is the standard library.\n\n```clpp\nlink @clpp.axiom as Axiom;\nlink \"./combat.clp\" as Combat;\n```"},
    KeywordDoc{"as", Kind::Keyword, "module · name of the imported module",
               "The name the module is used by.\n\n```clpp\nlink @clpp.window as Window;\n```"},
    KeywordDoc{"extern", Kind::Keyword, "declaration · native function of the host",
               "Declares a supported native signature with its result type first. The current extern registry exposes strlen(string) -> number.\n\n```clpp\nextern number strlen(string text);\n```"},
    KeywordDoc{"static_cast", Kind::Keyword, "numeric conversion",
               "Explicitly converts number and float values. Float-to-number truncates toward zero and rejects non-finite or out-of-range values.\n\n```clpp\nnumber count = static_cast<number>(3.9);\n```"},
    KeywordDoc{"signal", Kind::Keyword, "declaration · event",
               "A typed event. Connect returns a handle; ~>Connect registers with its internal caretaker; Clear disconnects managed handlers.\n\n```clpp\nsignal<number> died;\ndied~>Connect(showGameOver);\ndied::Fire(1200);\n```"},
    KeywordDoc{"observable", Kind::Keyword, "declaration · variable that reports changes",
               "OnChange returns a connection handle and invokes synchronous callbacks after each assignment in registration order. Disconnect releases the callback; Connected reports its state.\n\n```clpp\nobservable number coins = 10;\ncoins.OnChange(func (number v) -> void { post(v); });\ncoins = 25;\n```"},
    KeywordDoc{"atomic", Kind::Keyword, "declaration · counter shared between threads",
               "A value threads can share. Use `fetch_add` and `atomic_load`.\n\n```clpp\natomic hits = 0;\nfetch_add(hits, 1);\n```"},
    KeywordDoc{"operator", Kind::Keyword, "declaration · operator for a type",
               "Defines a typed operator for a data definition.\n\n```clpp\nMoney operator+(Money a, Money b) { return Money { cents = a.cents + b.cents }; }\n```"},
    KeywordDoc{"decltype", Kind::Keyword, "type of another variable",
               "Uses the type of an existing variable.\n\n```clpp\ndecltype(base) other = 4;\n```"},

    // --- modifiers ----------------------------------------------------------------------------
    KeywordDoc{"abstract", Kind::Keyword, "modifier · abstract definition/method",
               "An abstract definition cannot be constructed. Fieldless abstract contracts can accompany one storage base; concrete children implement each method.\n\n```clpp\nabstract def Shape { abstract float Area(); }\n```"},
    KeywordDoc{"override", Kind::Keyword, "modifier · replaces a parent method",
               "States that the method replaces an inherited method, preserving its callable contract and visibility.\n\n```clpp\noverride float Area() { return @this.Side * @this.Side; }\n```"},
    KeywordDoc{"final", Kind::Keyword, "modifier · cannot be inherited/overridden",
               "A final definition cannot be inherited; a final method cannot be overridden."},
    KeywordDoc{"private", Kind::Keyword, "modifier · access within the declaring definition",
               "Field or method reachable only from the declaring definition's methods.\n\n```clpp\ndef Account { private number Pin; }\n```"},
    KeywordDoc{"public", Kind::Keyword, "modifier · public access (default)", "Members are public by default; `public` makes it explicit."},
    KeywordDoc{"async", Kind::Keyword, "modifier · asynchronous function",
               "An async function produces a typed task, started with spawn and observed with await.\n\n```clpp\nasync string Load(string name) { return name; }\n```"},

    // --- control flow -------------------------------------------------------------------------
    KeywordDoc{"if", Kind::Keyword, "control flow · condition",
               "Runs the block when the condition is true. Braces are required.\n\n```clpp\nif (hp <= 0) { post(\"game over\"); } else { post(\"alive\"); }\n```"},
    KeywordDoc{"else", Kind::Keyword, "control flow · otherwise", "Block that runs when the `if` condition is false."},
    KeywordDoc{"while", Kind::Keyword, "control flow · loop with a condition",
               "Repeats while the condition is true.\n\n```clpp\nwhile (Window.Frame()) { … }\n```"},
    KeywordDoc{"for", Kind::Keyword, "control flow · loop",
               "`for (auto x in source)` walks numbers, lists, strings and dictionaries; the C style also exists.\n\n```clpp\nfor (auto i in 3) { post(i); }\nfor (auto i = 0; i < 10; i += 2) { post(i); }\n```"},
    KeywordDoc{"in", Kind::Keyword, "control flow · part of for … in", "`for (auto item in items) { … }`"},
    KeywordDoc{"do", Kind::Keyword, "control flow · tests after the body",
               "Runs the body at least once. Continue tests the condition; break exits immediately.\n\n```clpp\nnumber attempts = 0;\ndo { attempts += 1; } while (attempts < 3);\npost(attempts);\n```"},
    KeywordDoc{"break", Kind::Keyword, "control flow · leaves the loop", "Leaves the current `while`, `do`, `for` or `switch`."},
    KeywordDoc{"continue", Kind::Keyword, "control flow · next iteration", "Jumps to the next iteration of the loop."},
    KeywordDoc{"return", Kind::Keyword, "control flow · returns from the function", "Leaves the function, optionally with a value.\n\n```clpp\nreturn a + b;\n```"},
    KeywordDoc{"match", Kind::Keyword, "control flow · pattern matching",
               "Tries patterns in order; with enums, every case is required.\n\n```clpp\nmatch (state) {\n  Idle ~> post(\"idle\");\n  _ ~> post(\"other\");\n}\n```"},
    KeywordDoc{"guard", Kind::Keyword, "control flow: require a condition",
               "The else branch must exit. A successful null check narrows an optional variable.\n\n```clpp\nguard (score != null) else { return; }\npost(score + 1);\n```"},
    KeywordDoc{"switch", Kind::Keyword, "control flow · choice by value",
               "Compares with each `case`; there is no fall-through.\n\n```clpp\nswitch (key) {\n  case 1: post(\"jump\"); break;\n  default: post(\"idle\"); break;\n}\n```"},
    KeywordDoc{"case", Kind::Keyword, "control flow · case of a switch", "`case value: statements`"},
    KeywordDoc{"default", Kind::Keyword, "control flow · default case of a switch", "Runs when no `case` matches."},
    KeywordDoc{"try", Kind::Keyword, "errors · protects a block",
               "A runtime error inside the block jumps to `catch`.\n\n```clpp\ntry { post(xs[9]); } catch (e) { post(\"failed: \" .: e); }\n```"},
    KeywordDoc{"catch", Kind::Keyword, "errors · handles the error of try", "Receives the error message (the name is optional)."},
    KeywordDoc{"where", Kind::Keyword, "generics · type constraint",
               "Adds bounds to the declared type parameters, preserving their template bounds.\n\n```clpp\ntemplate <typename T>\nT OnlyInt(T value) where T: number { return value; }\n```"},

    // --- concurrency --------------------------------------------------------------------------
    KeywordDoc{"spawn", Kind::Keyword, "concurrency · starts a task",
               "Starts an `async` function and returns a `task` right away.\n\n```clpp\ntask<string> t = spawn load(\"map\");\n```"},
    KeywordDoc{"await", Kind::Keyword, "concurrency · waits for a task", "Waits for the `task` and returns its result.\n\n```clpp\npost(await t);\n```"},
    KeywordDoc{"parallel", Kind::Keyword, "concurrency · several tasks at once",
               "Starts several `async` calls, waits for all of them and returns the list of results.\n\n```clpp\nconst auto r = parallel(cost(2), cost(5));\n```"},
    KeywordDoc{"thread", Kind::Keyword, "concurrency · native thread",
               "Runs the function on an OS thread; `join` waits for the result.\n\n```clpp\nconst auto t = thread simulate(1000);\npost(join(t));\n```"},

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
