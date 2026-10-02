# 3. Variables and types

CL++ checks types before execution and infers them when possible.

| Declaration | Mutable? | Type |
| --- | --- | --- |
| `let name = value;` | No | Inferred |
| `let mut name = value;` | Yes | Inferred |
| `Type name = value;` | Yes | Explicit |
| `let name: Type = value;` | No | Explicit |

```clp
let gravity = 9.8;
let mut speed = 0;
int lives = 3;
speed += 5;
lives--;
post(speed);
```

Assigning to an immutable `let` is a compile error. `auto` infers a mutable variable's type, while `decltype(name)` uses another variable's type. `const` declares a fixed value; `constexpr` requires a value that can be computed at compile time.

## Built-in types

| Type | Meaning |
| --- | --- |
| `int` | Integer |
| `float`, `double` | 64-bit floating-point number |
| `bool` | Boolean |
| `string` | UTF-8 text |
| `Vector2`, `Vector3`, `Vector4` | Mathematical vectors |
| `buffer` | Byte buffer |
| `void` | No return value |

An `int` can be used where a `float` is expected. Division of two integers discards the fractional part; use a floating-point operand for fractional division. Later chapters cover structs, enums, variants, `Option<T>`, `Result<T,E>`, lists, arrays, dictionaries, and tasks.

## Values and moves

Structs, vectors, and lists have value semantics: assignment and function arguments copy them. Mutating a copy leaves the original intact. `move(value)` transfers a value and leaves the source empty, avoiding a copy for large strings and lists.

```clp
struct Stats { int hp; }
Stats first = Stats(100);
Stats second = first;
second.hp = 1;
post(first.hp);  << 100
```

## Which declaration should you choose?

Start with `let` for a value that stays fixed in its scope. Choose `let mut` when the same binding must change, such as a counter or a player's current HP. Write an explicit type when it makes an API boundary clearer or inference cannot determine the intended type. Use `const` for a named fixed value shared across code, and `constexpr` when that value must be available during compilation. Avoid making every binding mutable: accidental assignments are then harder for the compiler to catch.

Value copies are helpful for snapshots, undo operations, and isolated calculations. For a large collection that should change ownership, `move` avoids copying; do not use the source binding after moving it. A method that mutates its receiver is often clearer than copying and returning an entire entity for every small update.
