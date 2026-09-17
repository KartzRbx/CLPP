---
title: Operators
---

# Operators

Memorize this table. Almost every bug in new CL++ code is the wrong accessor.

| CL++ | Luau | When |
| --- | --- | --- |
| `obj.Prop` | `obj.Prop` | property |
| `obj::Method(a)` | `obj:Method(a)` | method call |
| `obj::Child` (no call) | `obj.Child` | scope / nested name |
| `Table:Key` | `Table.Key` | dictionary / module table |
| `a .: b` | `a .. b` | strings |
| `signal~>Connect(fn)` | janitor-managed `Connect` | cleanup |
| `!=` `&&` `\|\|` `!` | `~=` `and` `or` `not` | logic |
| `++` `--` | `+= 1` `-= 1` | increment |

## Worked examples

```clpp
players::PlayerAdded::Connect(fn);     // players.PlayerAdded:Connect(fn)
player::FindFirstChild("x");           // player:FindFirstChild("x")
DataService:Server::WaitFor(p);        // DataService.Server:WaitFor(p)
player.Name .: "_key";                 // player.Name .. "_key"
for (Player* p : players::GetPlayers()) { }
```

In a C-style `for (int i = 0; i < n; i++)`, the `;` separates clauses. The `:` of range-for is **only** `for (T name : collection)`.

## Arithmetic

`+` `-` `*` `/` work on numbers. `+` does **not** concatenate. `*` is multiply, never pointer deref.

## Assignment

`=` `+=` `-=` `*=` `/=` emit the same operators. Designated initializers:

```clpp
DataService:Server::Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true,
});
```

Next: [Variables and scope](variables).
