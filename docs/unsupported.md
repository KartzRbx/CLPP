---
title: What CL++ does not do
---

# What CL++ does not do

CL++ is a **subset** of C++ with Luau semantics. These features are out of scope on purpose.

- Full ISO C++ (template specialization, SFINAE, RAII `delete`, `std::`)
- Function **overloading** and **default arguments** (decision: forbidden for now)
- `->`, `int&`, address-of, pointer arithmetic (write `Player player`)
- C++ lambda captures `[x]`, `[&]`
- JSX
- Macros other than `#pragma once` / `strict` / `nonstrict` / `native` / `optimize`
- Connecting Roblox APIs for you — that is [Cluaupp](cluaupp)
- Using `#include` as the **language** module system (use [`import`](modules))

## In the language (do not “add” these as missing)

`using` / `type` aliases · unions `A | B` · intersections `A & B` · `interface` · `enum` / `enum class` · `continue` · ternary · `do/while` · `try/catch` · checked generics `template <typename T : Bound>` · `comptime` reflection · `import { } from`

Bare `func` still emits `(...any) -> any` — that is a **known weakness**, not a feature. Prefer typed named functions ([TYPE_SYSTEM](architecture/TYPE_SYSTEM)).

Prefer the [cheat sheet](cheatsheet) surface.
