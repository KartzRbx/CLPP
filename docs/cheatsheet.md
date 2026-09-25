---
title: Language cheat sheet
---

# Language cheat sheet

## Files

`.clh` types/prototypes · `.clp` module · `.clpp` script · `.server` Script · `.client` LocalScript

## Modules

`link @clpp.roblox` · `link @clpp.libs.janitor as Janitor` · `link @game.ReplicatedStorage.Modules.Combat as Combat` · `link "./path.clh" as Name`

## Access

`.` property / instance method · `:` type / protected call · `::` static / manual Connect · `[]` index · `.:` concat · `~>` Janitor Connect/Once

## Text

`"double"` · `'single'` · `` `template {expr}` ``

## I/O

`post` · `warn` · `report` · `null` · `to_string` / `to_number` / `to_bool`

## Types

`int` `float` `bool` `string` `void` `func` (lossy) · `auto` · `optional<T>` · `array<T>` · `dictionary<K,V>` · `type` / `using` · `A | B` · `A & B` · `enum class` · `template <typename T : Bound>` · `comptime { }`

## Control

`if` `else` · `while` · C-for · `for (T x in xs)` · `switch` · `guard` · `match` · `break` · `continue` · `return`

## Concurrency

`async` `await` · `spawn { }` · `parallel { }` · `auto [a,b] = pcall(...)`

## OOP

`struct` / `class` / `interface` · `Class::Method` · `@this` / `@field` · `public:` / `private:` · parent `: Base`

## Not in the language

Overloading · default args · C++ captures · pointers · macros (beyond pragmas) — [unsupported](unsupported)
