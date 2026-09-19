---
title: Language cheat sheet
---

# Language cheat sheet

## Files

`.clh` header · `.clp` module · `.clpp` script · `.server` Script · `.client` LocalScript

## Access

`.` property / instance method · `:` type / protected call · `::` static / manual Connect · `[]` index · `.:` concat · `~>` Janitor Connect/Once

## Text

`"double"` · `'single'` · `` `template {expr}` `` · `` `a`, expr, `b` ``

## I/O

`post` output · `warn` warning · `report` error · `null` empty value · `to_string` / `to_number` / `to_bool` convert

## Types

`int` `float` `double` `bool` `string` `void` `func` `auto` `optional<T>` `array<T>` `dictionary<K,V>` `Player` `signal<T...>` `observable T`

## Control

`if` `else if` `else` · `while` · C-for · range-for `for (T x in xs)` · `switch` · `guard` · `match { Type t => }` · `break` · `return`

## Concurrency

`async` `await` · `spawn { }` · `parallel { }` · `auto [a,b] = pcall(...)`

## Events

`.Fire` `::Connect` (manual) `~>Connect` `~>Once` · `.OnChange` on observables · `GetPropertyChangedSignal`

## OOP

`struct` in `.clh` · `Class::Method` in `.clpp` · `@this` / `@field` → `self` · `void init()` · `new Class(parent)` · `GetService<T>()`

## Attributes

`[[server]]` `[[client]]`

## Pragma

`#pragma strict` `#pragma nostrict` `#pragma native` `#pragma optimize` `#pragma optimize 2` `#pragma once`

## Forbidden (by design)

`->` · `Player*` · `continue` · ternary · `do/while` · `try/catch` · `goto` · pointer arithmetic · C++ captures · `func []` · generic templates · `std::`

Generated API: [Builtins](/CLPP/api/Builtins) · [Operators](/CLPP/api/Operators) · [Signals](/CLPP/api/Signals)

Header-style index (every utility): [Language reference](reference)
