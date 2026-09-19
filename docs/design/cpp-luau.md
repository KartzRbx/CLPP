# Design: C++ → Luau

CL++ is the **language** (syntax, semantics, OOP): a C++-inspired subset aimed at Roblox scripts. The compiler emits Luau. Engine API wiring (generated headers, runtime, services) belongs to **Cluaupp**, not CL++.

The study reference is real leaderstats code (`LeaderstatsServer`, `PlayerData`) and the C++ → Luau handbook.

## Goals

1. Write like C++ (types, `struct`, `new`, `GetService<T>`), with our own operators: `.` property/instance method, `:` type/protected call, `::` static/manual Connect, `~>` Janitor, `.:` concatenation.
2. Emit modern Luau (`local`, `const`, `const function`, `Instance.new`, `game:GetService`).
3. One input file becomes one output file (Rojo infers Script / LocalScript / ModuleScript from the name).
4. Own extensions: `.clh`, `.clp`, `.clpp` — not `.h` / `.cpp`.

## What we are not

CL++ is **not** a full C++ compiler and **not** the Roblox API connector. There is no `std::`, pointer arithmetic, `delete`, overloading, generic templates (beyond the mapped ones), `continue`, ternary, `do/while`, `try/catch`, or `goto`.

`Player` is a Roblox Instance of class `Player`. Do not write `Player*` or `->`. Access is `player.Name` / `player.FindFirstChild(...)`.

Range-for uses `in` between the name and the collection: `for (Player player in players.GetPlayers())`. The C++-style `:` in `for (T x : xs)` is the same loop, not a protected call.

## Study files

| Role | CL++ equivalent |
| --- | --- |
| `struct` + prototypes | `LeaderstatsServer.clh` |
| `Class::` + `void init()` | `LeaderstatsServer.server.clpp` |
| Template structs | `PlayerData.clh` |

## Matching stem

`LeaderstatsServer.clh` next to `LeaderstatsServer.server.clpp`. An include with the same stem is **inlined**. An include with another name becomes `require`.
