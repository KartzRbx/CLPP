# Types and variables

Typed and inferred declarations, constants, optional narrowing and explicit numeric
casts are VM-tested. A `target` block is a future or separate contract, not a
passing compiler test. Official CL++ execution uses its own VM.

## Declare a variable

The **type comes first**, followed by the name and initial value:

```clp
number health=100; string name="Ana"; bool alive=true; post(health); post(name); post(alive);
```

```output
100
Ana
true
```

Read `number health = 100;` as: create an integer named `health`, initialized to one hundred. The shape is `Type name = value;`.

Use a type or `auto`; no extra binding keyword is needed to make an ordinary variable changeable. Proposed migration diagnostic identifiers are not release guarantees.

## Primitive types

| Type | Holds | Example |
| --- | --- | --- |
| `number` | Whole numbers | `42` |
| `float` | Fractional measurements | `3.14` |
| `bool` | True or false | `true` |
| `string` | Text | `"hello"` |
| `void` | No returned value | Used as a function result type |

`number` stores signed 64-bit integers exactly; arithmetic overflow is an error. `float` uses 64-bit floating-point representation. Conversion from `number` to `float` can lose precision for large integers.

### Why distinguish integers and fractional values?

The compiler can check that a function expecting an integer count is not accidentally given a fractional measurement. Implicit decimal-to-integer assignment is rejected; explicit numeric casts follow the rules below. Static checking is not a universal zero-cost performance guarantee. See the [mental model](./02-modelo-mental.md).

If you write a transpiler in CL++ that emits Luau, both numeric types might map to Luau `number`. That application must still preserve CL++ behavior; it is not an official runtime mapping.

## `auto`: infer the type

```clp
def Player { string Name; number Health=100; } auto ana=Player { Name="Ana" }; post(ana.Name); post(ana.Health);
```

```output
Ana
100
```

Use inference when the initializer already communicates the type. A typed factory function is another clear initializer; a host-specific example such as `GetService<Players>()` would require APIs supplied by that application's author.

Write types explicitly in **parameters and fields**: they communicate the contract of your code. The supplied design also recommends an explicit function result.

```clp
number Double(number value) { return value*2; } post(Double(21));
```

```output
42
```

## `const`: an immutable value

```clp error
const number MAX_HEALTH = 100;
MAX_HEALTH = 200; << Expected error: a constant cannot be reassigned.
```

```output
cannot assign to immutable binding
```

Use `const auto` when the initializer should choose the type of a fixed binding:

```clp
const auto limit = 100;
const auto title = "CL++";
post(limit);
post(title);
```

```output
100
CL++
```

Use constants for limits, rates, fixed names, and other shared rules. An uppercase name with underscores is a useful convention, not a type-system requirement.

> **Changeable by default.** Use `const` when a binding should not change. The target model also makes ordinary `auto` variables changeable; `auto` and typed variables are mutable in this checkout; `const` rejects reassignments.

The approved language contract makes the contents immutable as well. Applying `const` to a shared object or collection prevents mutation through every alias, including nested objects and collections. `const auto` retains the concrete inferred type: `const auto limit = 100` has type `number`, and `const auto player = Player { ... }` has type `Player`. Inference is a compile-time operation; it does not replace the value type with an unchecked generic type.

## `optional<T>` and `null`: a value that may be absent

A lookup might fail to find a player. The return type should communicate that possibility:

```clp
def Player { string Name; }
optional<Player> FindPlayer(string name) { if(name=="Ana") { return Player { Name=name }; } return null; }
post(FindPlayer("unknown")==null); optional<Player> found=FindPlayer("Ana"); if(found!=null) { post(found.Name); }
```

```output
true
Ana
```

The checker rejects access before the value has been narrowed:

```clp error
def Player { string Name; }
optional<Player> found=null;
post(found.Name);
```

```output
unknown field
```

The lookup above supplies a minimal executable registry rule, including a missing name. Other languages can also express optional types; the lesson is to keep that contract visible in CL++.

### Three ways to handle absence

**1. `if`: check for null and narrow inside the branch.**

```clp
def Player { string Name; }
optional<Player> found=Player { Name="Ana" };
if(found!=null) { post(found.Name); }
```

```output
Ana
```

**2. `guard`: leave early if the value is missing.**

```clp
def Player { string Name; }
void Show(optional<Player> found) { guard(found!=null) else { return; } post(found.Name); }
Show(null); Show(Player { Name="Ana" });
```

```output
Ana
```

**3. Compare explicitly with `null`.**

```clp
def Player { string Name; }
optional<Player> found=null;
if(found!=null) { post(found.Name); } else { post("missing"); }
```

```output
missing
```

Use `guard` inside a function when an absent value means there is nothing more to do. It keeps the main path readable without deeply nested conditionals.

> **An `optional<bool>` needs care.** Use `option != null` to test presence even when its contained value is false. An unqualified truthiness check is not a presence check. To ask whether the value is true, compare explicitly: `option == true`.

## Conversions

Numeric conversions follow these checking rules:

| From → To | Rule |
| --- | --- |
| `number` → `float` | Allow conversion; large integers may round |
| `float` → `number` | Require an explicit cast; truncate toward zero and reject out-of-range values |
| `optional<T>` → `T` | Reject unless the value has been narrowed |

Examples of proposed explicit conversion APIs:

```clp target
string text = to_string(42); << Intended text: "42".
number parsed = to_number("42"); << Return type and failure behavior need verification.
bool flag = to_bool("x"); << Conversion semantics need verification.
float fraction = static_cast<float>(10);
```

```clp
number count = static_cast<number>(-3.9);
float fraction = static_cast<float>(7) / 2;
post(count);    << -3
post(fraction); << 3.5
```

`static_cast<number>` truncates toward zero and reports an error for non-finite or out-of-range values. `static_cast<float>` converts a number to a decimal, with possible rounding for large integers. Numeric casts reject string, boolean, null and object operands. The string conversion APIs shown above still need a separate contract; an `as` coercion is not specified.

## Literals

```clp
post(42); post(3.14); post("text"); post(true); post(false); post(null);
```

```output
42
3.14
text
true
false
null
```

A literal supplies a value directly; a type name states which values are permitted. See [Operators](./04-operadores.md) for calculations using those values.

## Quick reference

| Goal | Write |
| --- | --- |
| Store a whole number | `number count = 5;` |
| Infer from an initializer | `auto item = Item {};` |
| Keep a binding fixed | `const number MAX = 10;` |
| Allow absence | `optional<T>`; `T?` remains a proposed shorthand |
| Handle absence before use | `guard (value) else { return; }` inside a function |
| Express text conversion | `to_string(value)` in the target API |

## File scope and initialization

A declaration directly at the top of a file creates a file binding. Functions, methods and callbacks in that file access the same binding; parameters and block-local declarations can shadow its name. Every file binding needs an explicit initializer. Initializers and entry statements execute in source order, after imported dependencies finish initializing. A call that reads a binding before its initializer has run raises an error.

```clp
number score = 0;
number Add(number amount) { score += amount; return score; }
post(Add(20));
post(Add(22));
post(score);
```

```text
20
42
42
```

`auto` retains the concrete inferred type. Computed constants use the same initialization rules and `const` freezes reachable data through all aliases. Workers share file state; synchronize compound updates explicitly. Destructive `move` from a shared file binding is rejected; local bindings keep their local move behavior. Each program run starts with fresh file state.

