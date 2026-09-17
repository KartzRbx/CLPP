---
title: Collections
---

# Collections

## array

```clpp
array<string> names = {"Kartz", "Player1"};
```

```luau
local names: {string} = { "Kartz", "Player1" }
```

`vector<T>`, `LuaArray<T>`, and `span<T>` mean the same thing.

## dictionary

```clpp
dictionary<string, int> stats = {
    {"Coins", 100},
    {"Gems", 50}
};
```

```luau
local stats: { [string]: number } = { Coins = 100, Gems = 50 }
```

Table keys in expressions use `:` (`stats:Coins` if you go through a table API). Instance properties stay `.`.

These tables are what Fusion and Vide property lists look like: string keys to values. See [Fusion](ui-fusion) and [Vide](ui-vide).

Next: [Structs and methods](oop).
