---
title: What CL++ does not do
---

# What CL++ does not do

CL++ is a **subset**. If you reach for these, rewrite the idea in Luau terms.

- Full ISO C++ (templates, overloading, RAII `delete`, `std::`)
- `->`, `int&`, address-of, pointer arithmetic (write `Player player`)
- `continue`, ternary `? :`, `do/while`, `try/catch`, `goto`
- C++ lambda captures `[x]`, `[&]`, `func [](…)`, `[]() { }`
- `@this` / `@field` outside `Class::Method`
- JSX
- Macros other than `#pragma once` / `strict` / `nostrict` / `native` / `optimize`
- Connecting Roblox APIs for you — that is [Cluaupp](cluaupp)

The compiler will either skip the construct (`using`, `namespace`, `enum` declarations) or fail to parse. Prefer the [cheat sheet](cheatsheet) surface.
