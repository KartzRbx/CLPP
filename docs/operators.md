---
title: Operators
---

# Operators

Memorize this table. Almost every bug in new CL++ code is the wrong accessor.

| CL++ | When | Example | Luau |
| --- | --- | --- | --- |
| `.` | Properties **and** instance methods (the default) | `player.Name`, `player.Kick()`, `workspace.FindFirstChild("x")` | `.` / `:` |
| `:` | Variable/function types **and** protected calls | `age: int = 10`, `player:Kick()` | Luau `:` types / `pcall` |
| `::` | Static scope **and** manual connections (no Janitor) | `Vector3::new`, `players.PlayerAdded::Connect(fn)` | `.` / `:` |
| `~>` | Signal connections with Janitor | `players.PlayerAdded~>Connect(fn)` | `janitor:Add(..., "Disconnect")` |
| `[]` | Indexing arrays, maps, and dynamic child names | `items[0]`, `data["Coins"]`, `workspace["Name"]` | `[key]` |
| `.:` | String join | `player.Name .: "_key"` | `..` |
| `!=` `&&` `\|\|` `!` | Logic | | `~=` `and` `or` `not` |
| `++` `--` | Increment | | `+= 1` `-= 1` |

Anonymous listeners are `func (params) { }`, not C++ lambdas. `[]` is **only** indexing (arrays, maps, dynamic child names) — never a capture list.

## Worked examples

```clpp
player.Name = "Kartz";                 // player.Name
player.Kick();                         // player:Kick()
workspace.FindFirstChild("x");         // workspace:FindFirstChild("x")
age: int = 10;                         // local age: number = 10
player:Kick();                         // pcall → nil on error, never throws
players.PlayerAdded::Connect(fn);      // players.PlayerAdded:Connect(fn)  (you Disconnect)
players.PlayerAdded~>Connect(fn);      // janitor:Add(Connect, "Disconnect")
items[0];                              // items[0]
playerData["Coins"] = 600;             // playerData["Coins"] = 600
workspace["Baseplate"];                // workspace["Baseplate"]
task::wait(1);                         // task.wait(1)
DataService.Server.WaitFor(p);         // DataService.Server:WaitFor(p)
for (Player p in players.GetPlayers()) { }
```

`player.Kick()` is the normal method call (it passes `self` in Luau). `player:Kick()` is the same call wrapped in `pcall`: on error it returns `nil` instead of stopping the script.

In a C-style `for (int i = 0; i < n; i++)`, the `;` separates clauses. The `:` of range-for is **only** `for (T name : collection)` — prefer `for (T name in collection)`.

## Arithmetic

`+` `-` `*` `/` work on numbers. `+` does **not** concatenate. `*` is multiply, never pointer deref.

## Assignment

`=` `+=` `-=` `*=` `/=` emit the same operators. Designated initializers:

```clpp
DataService.Server.Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true,
});
```

Next: [Variables and scope](variables).
