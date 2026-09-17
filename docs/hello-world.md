---
title: Your first script
---

# Your first script

Create `hello.server.clpp`. The `.server` tag means Rojo will treat the output as a **Script**.

```clpp
#include <clpp/roblox.clh>

void Greet(Player* player) {
    post("Player name: " .: player.Name);
}

void init() {
    Players* players = GetService<Players>();

    for (Player* player in players::GetPlayers()) {
        Greet(player);
    }

    players::PlayerAdded::Connect(func [](Player* playerEntered) {
        post("New player connected: " .: playerEntered.Name);
    });
}
```

## Line by line

1. `#include <clpp/roblox.clh>` — IntelliSense for engine types. No Luau is emitted for this header.
2. `void Greet(Player* player)` — a procedure. `Player*` is an Instance, not a C pointer.
3. `player.Name` — **property** (dot).
4. `"Player name: " .: player.Name` — **concatenation** (`.:` → Luau `..`).
5. `void init()` — runs at the end of Scripts and LocalScripts.
6. `GetService<Players>()` — typed service lookup.
7. `for (Player* player in players::GetPlayers())` — range-for. `in` is the collection; not a table key.
8. `players::PlayerAdded::Connect` — `::` is method/scope. Emitted `players.PlayerAdded:Connect`.
9. `func [](Player* playerEntered) { ... }` — lambda. Captures are empty `[]`; Luau closures still see outer locals.

Compile:

```bash
clpp compile hello.server.clpp
```

You get Luau with `const function`, `game:GetService("Players")`, and an `init()` call at the bottom.

Next: [Mental model](mental-model).
