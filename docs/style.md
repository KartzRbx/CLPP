---
title: Style guide
description: How to format CL++ so it matches the compiler, the editor pack, and this site.
---

CL++ is not C++. Write the forms this compiler emits. The editor pack (`clpp install`) highlights them.

## Files

- Headers: `.clh`. Modules: `.clp` / untagged `.clpp`. Scripts: `.server.clpp` / `.client.clpp`.
- One service type per pair: `LeaderstatsServer.clh` next to `LeaderstatsServer.server.clpp`.
- `#include <clpp/roblox.clh>` for engine names. `#include <clpp/libs/janitor.clh>` when you use `~>`.

## Types

- Instances are class names: `Player player`, `Players players`, `BasePart part`.
- There is no address-of and no `->`. Properties and instance methods use `.`.
- Prefer `for (Player player in players.GetPlayers())` over the C++ `:` range-for.

## OOP

- Define methods with `Class::Method`. Call them with `.` (`hello.Greet(player)`).
- Inside a method, `@this` is the object (`self`). `@janitor` is `self.janitor`. Bare fields still become `self.field`.
- Do not put `@this` on the parameter list. `::` already injects the receiver.
- `this` without `@` is the same alias. Prefer `@this`.

```clpp
void CombatServer::BindPart(BasePart part) {
    @janitor.Add(part, "Destroy");
    other.Register(@this);
}
```

## Operators

| Write | Do not write |
| --- | --- |
| `"hi " .: name` | `"hi " .. name` or `"hi " + name` |
| `players.PlayerAdded~>Connect(fn)` | `::Connect` unless you will Disconnect yourself |
| `func (Player player) { }` | `[]() {}` or `func [](…)` |
| `name.Fire(...)` | `name::Fire` |
| `player.Kick()` | `player->Kick()` |

## Scripts

- Entry is `void init()`. There is no `int main()`.
- Construct **one** service object in `init()` and close over it from callbacks.
- `post` / `warn` / `report` — not `print` / `error` / `tostring`. Use `to_string` / `to_number` / `to_bool`.

## Editor

Reload the window after `clpp setup` / `clpp install`. `@` is the receiver sigil; `@this` and `@field` color differently.
