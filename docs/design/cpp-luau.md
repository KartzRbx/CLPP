# Design: C++ → Luau

CL++ is the **language** (syntax, semantics, OOP): a C++-inspired subset aimed at Roblox scripts. The compiler emits Luau. Engine API wiring (generated headers, runtime, services) belongs to **Cluaupp**, not CL++.

## Goals

1. Write like C++ (types, `struct`, `new`, `GetService<T>`), with our own operators: `.` property/instance method, `:` type/protected call, `::` static/manual Connect, `~>` Janitor, `.:` concatenation.
2. Emit modern Luau (`local`, `const`, `Instance.new`, `game:GetService`).
3. One input file becomes one output file (Rojo infers Script / LocalScript / ModuleScript from the name).
4. Own extensions: `.clh`, `.clp`, `.clpp` — not `.h` / `.cpp`.
5. Language modules use `link` ([modules](../modules)).

## What we are not

Not a full ISO C++ compiler and not the Roblox API connector. No `std::`, pointer arithmetic, `delete`, **overloading**, or C++ template specialization/SFINAE.

**In** the language today (do not list as missing): `continue`, ternary, `do/while`, `try/catch`, `enum`, `type`/`using`, unions, checked generics (`template <typename T : Bound>`), `comptime`.

`Player` is a Roblox Instance. Access is `player.Name` / `player.FindFirstChild(...)`. There is no `->`.

## Study files

| Role | CL++ equivalent |
| --- | --- |
| Types / prototypes | `PlayerData.clh` + `link "./PlayerData.clh" as PlayerData` |
| `Class::` + `void init()` | `.server.clpp` |
| Checked generics | RFC 0010 samples in tests |

## Matching stem (legacy)

`link "./Foo.clh" as Foo` is how a `.clpp` pulls its header. Angle includes are not a module form.
