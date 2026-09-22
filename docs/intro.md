---
title: What is CL++?
---

# What is CL++?

CL++ is a **programming language**. You write C++-inspired source; the `clpp` compiler emits [Luau](https://luau.org) for Roblox. Code samples on this site use the language id [`clpp`](reference/language-id) — not `cpp`.

It is **not** a full C++ compiler and **not** a Roblox SDK. [Cluaupp](https://github.com/KartzRbx/Cluaupp) owns engine headers, Rojo, and project `init`. This site teaches the **language**: syntax, types, modules, OOP, and every construct the compiler understands.

## What you will be able to do

By the end of this course you will:

1. Install `clpp` and the editor pack.
2. Write `.clh` / `.clp` / `.clpp` files and know which Roblox instance they become.
3. Connect files with [`import { Name } from "path"`](modules).
4. Use `.` for the default, `:` for types and safe calls, `::` for static/manual Connect, `~>` for Janitor.
5. Build services with `struct`, `guard`, `match`, `observable`, and `signal`.
6. Read the **[language reference](reference)** the same way you would read a C++ reference.

## A tiny program

```clpp
struct HelloServer {
    void Greet(Player player);
};

void HelloServer::Greet(Player player) {
    post("Player name: " .: player.Name);
}

void init() {
    HelloServer hello;
    Players players = GetService<Players>();
    post("online: " .: players.GetPlayers());
}
```

`void init()` is the script entry point. There is no `int main()`. `post` is print. `GetService<Players>()` is `game:GetService("Players")`. Engine IntelliSense still comes from Cluaupp headers when the host injects them — not from inventing `#include` as the module system.

## Course map

| Part | You learn |
| --- | --- |
| Start here | Install, hello world, mental model, [modules](modules), [style](style) |
| The language | Types, operators, functions, OOP |
| Modern syntax | Signals, guard, match, async, attributes |
| UI & advanced | Fusion, Vide, advanced types, [benchmarks](benchmarks) |
| Reference | [Language reference](reference), cheat sheet, unsupported list |
| Specification | [Compiler pipeline](spec/compiler), [type system](architecture/TYPE_SYSTEM), EBNF |

Next: [Install the compiler](install).
