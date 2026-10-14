# 3. Variables and types

This page teaches the current CL++ declaration and type rules. Examples explicitly
marked `target` remain future proposals or separate contracts and are not counted
as compiler tests. See [implementation status](../reference/status.md).

A variable gives a value a name. A declaration creates the name; an assignment changes the value of an existing variable. CL++ checks types before execution.

## Declare a variable

| Form | Changes allowed? | Type selection |
| --- | --- | --- |
| `number score = 0;` | Yes | Explicit type before the name |
| `auto speed = 2.5;` | Yes | Inferred from the initial value |
| `const string title = "CL++";` | No | Explicit type before the name |
| `const number maxLives = 3;` | No | Explicit type before the name |
| `const auto maxLives = 3;` | No | Inferred from the initial value |
| `constexpr number tileSize = 8 * 2;` | No | Numeric expression evaluated during compilation |
| `decltype(score) saved = 0;` | Yes | Uses the type of another variable |

The ordinary shape is **type, name, `=`, value, `;`**. Always initialize the variable. `auto` keeps the declaration short; it does not allow the variable's type to change later. With `const`, write `const`, an explicit type or `auto`, the name, `=`, the initial value, and `;`.

```clp
const number maxLives = 3;
number lives = maxLives;
auto speed = 0;
speed += 5;
lives--;
post(speed);
post(lives);
```

```text
5
2
```

Use `const` when a value should stay fixed, a typed declaration when the intended type matters, and `auto` when the initializer makes it obvious. `constexpr` requires an explicit type or `auto`, and a literal value or arithmetic over numeric literals. Integer folding remains exact and checks overflow and division by zero. A floating destination does not change integer division inside the initializer: `constexpr float value = 7 / 2;` is 3.

```clp
constexpr number tileSize = 8 * 2;
constexpr auto exact = 9007199254740993 + 2;
constexpr float fraction = 7.0 / 2;
constexpr bool enabled = true;
post(tileSize);
post(exact);
post(fraction);
post(enabled);
```

```output
16
9007199254740995
3.5
true
```

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

## Assignment

`lives = 2;` replaces an existing value. `lives += 1;` adds to it; `lives++;` increases it by one. The left-hand name must already exist and permit assignment. Assignment is a statement, not a way to declare another variable.

```clp error
const number limit = 3;
limit = 4; << Invalid: assignment to a constant must be rejected.
```

```output
cannot assign to immutable binding
```

## Built-in types

| Type | Use |
| --- | --- |
| `number` | Whole numbers: scores, counts, indices |
| `float` | Measurements using 64-bit floating-point values |
| `bool` | `true` or `false` |
| `string` | UTF-8 text |
| `Vector2`, `Vector3`, `Vector4` | Mathematical vectors |
| `buffer` | Bytes |
| `task` | A task handle |
| `void` | A function with no return value |

A `number` can be used where a `float` is expected. Integer division discards the fractional part; a floating-point operand enables fractional division. Types are checked, even when a declaration uses `auto`.

Booleans retain a distinct runtime kind in objects, collections, events, tasks and JSON. `true` is distinct from the number `1`; numeric APIs do not accept a boolean as a count, index or resource handle. Native boolean parameters require actual booleans.

```clp
number whole = 7 / 2;
float fraction = 7.0 / 2;
bool ready = true;
string label = "score";
post(whole);
post(fraction);
post(ready);
post(label);
```

## Numeric precision and conversions

`number` stores integers from `-9223372036854775808` through `9223372036854775807` exactly. Addition, subtraction, multiplication and integer division report overflow instead of wrapping or silently converting to a decimal. Division by zero is an error. `float` uses 64-bit floating-point values; conversion from a large integer can round it.

```clp
number exact = 9007199254740993;
number count = static_cast<number>(3.9);
float fraction = static_cast<float>(7) / 2;
post(exact);    << 9007199254740993
post(count);    << 3
post(fraction); << 3.5
```

An explicit `static_cast<number>` truncates a fractional value toward zero. It rejects infinity, NaN and values outside the integer range. A decimal cannot be assigned implicitly to `number`. Numeric casts accept numeric operands; they do not parse strings or turn arbitrary objects into numbers.

## Scope and values

A block creates a scope. A variable declared inside a function or conditional belongs to that body. Pass values as parameters or return a result to share them with another function. Top-level declarations belong to the file's module; CL++ uses `.clp` modules without header files.

`def` objects, arrays and dictionaries have shared identity. Assignment and parameter passing retain the same object; mutations are visible through aliases. Primitive numbers and booleans remain ordinary values. `const` freezes reachable data contents through all aliases. Aggregate `==` compares identity; scalar `==` compares values.

```clp
def Stats { number Hp = 100; }
Stats first = Stats {};
Stats second = first;
second.Hp = 1;
post(first.Hp); << Intended result: 1, the same object.
```

```output
1
```

Construct another object or collection when independent state is required. A new outer collection may still contain shared object references; transfer and explicit deep-copy APIs require their own contracts. Read [data definitions](07-structs-and-inheritance.md), [functions](06-functions.md), and [modules](10-modules.md).
