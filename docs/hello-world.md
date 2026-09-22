---
title: Your first script
---

# Your first script

Create `hello.server.clpp`. The `.server` tag means Rojo will treat the output as a **Script**.

```clpp
struct HelloServer {
    void Greet(Player player);
};

void HelloServer::Greet(Player player) {
    post("Player name: " .: player.Name);
}

void init() {
    HelloServer hello;
    Players players = GetService<Players>();

    for (Player player in players.GetPlayers()) {
        hello.Greet(player);
    }

    players.PlayerAdded~>Connect(func (Player playerEntered) {
        hello.Greet(playerEntered);
    });
}
```

## Line by line

1. `struct HelloServer` + `void HelloServer::Greet` — define methods with `::`. Call them with `.` (`hello.Greet(player)`).
2. `player.Name` — **property** (dot).
3. `"Player name: " .: player.Name` — **concatenation** (`.:` → Luau `..`).
4. `void init()` — runs at the end of Scripts and LocalScripts.
5. `PlayerAdded~>Connect` — Janitor-style connection.
6. Shared types from another file use [`import { … } from`](modules), not language `#include`.

Platform headers (`<clpp/roblox.clh>`) remain a **Cluaupp** concern when the host wants IDE dump types.

## Multi-file

```clpp
import { Wallet } from "./PlayerData.clh";
```

See [Modules](modules) · [Files](files).
