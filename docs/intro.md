---
title: What is CL++?
---

# What is CL++?

CL++ is a **programming language**. You write C++-inspired source; the `clpp` compiler emits [Luau](https://luau.org) for Roblox. Code samples on this site use the language id [`clpp`](reference/language-id) — not `cpp`.

It is **not** a full C++ compiler and **not** a Roblox SDK. [Cluaupp](https://github.com/KartzRbx/Cluaupp) owns engine headers, Rojo, and project `init`. This site teaches the **language**: syntax, types, OOP, and every construct the compiler understands.

## What you will be able to do

By the end of this course you will:

1. Install `clpp` and the editor pack.
2. Write `.clh` / `.clp` / `.clpp` files and know which Roblox instance they become.
3. Use `.` for the default, `:` for types and safe calls, `::` for static/manual Connect, `~>` for Janitor.
4. Build services with `struct`, `guard`, `match`, `observable`, and `signal`.
5. Drive UI with Fusion and Vide in CL++.
6. Read the **[language reference](reference)** the same way you would read [cplusplus.com/reference](https://cplusplus.com/reference/) — every utility has syntax, parameters, emit, and an example. The **API** tab groups the same surface as Moonwave classes.

## A tiny program

```clpp
#include <clpp/roblox.clh>

void init() {
    Players* players = GetService<Players>();
    post("online: " .: players.GetPlayers());
}
```

`void init()` is the script entry point. There is no `int main()`. `post` is print. `GetService<Players>()` is `game:GetService("Players")`.

## Course map

| Part | You learn |
| --- | --- |
| Start here | Install, hello world, mental model |
| The language | Types, operators, functions, OOP, Roblox |
| Modern syntax | Signals, guard, match, async, attributes |
| UI & advanced | Fusion, Vide, advanced types, production patterns |
| Reference | [Language reference](reference) (every utility), cheat sheet, unsupported list, emit spec |

Next: [Install the compiler](install).
