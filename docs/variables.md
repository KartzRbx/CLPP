---
title: Variables and scope
---

# Variables and scope

```clpp
int n = 0;                    // local number
age: int = 10;                // same, postfix type
const int MAX = 10;           // Luau const
observable int coins = 100;   // IntValue; assign to fire Changed
signal<Player*, int> OnPay;   // BindableEvent wrapper
auto* players = GetService<Players>();
```

## Where a name lives

- File-level declarations → file `local` / `const`.
- Inside a function → from that line to the end of the block. Nested `{ }` may shadow.
- Inside `Class::Method` → `@this` / `this` is `self`. `@field` and bare field names become `self.field`. Parameters shadow fields.

## Globals that stay bare

`post`, `warn`, `report`, `game`, `workspace`, `task`, library types, Instance classes, datatypes.

## Lifetime

Leaving a `{ }` block does **not** `Destroy` Instances. Use Janitor, `~>Connect`, or an explicit `.Destroy()`.

## Destructuring

```clpp
auto [ok, result] = pcall(func () {
    return 1;
});
```

Emits `local ok, result = pcall(...)`.

Next: [Functions and callbacks](functions).
