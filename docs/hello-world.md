---
title: Your first script
---

# Your first script

Create `hello.server.clpp`. The `.server` tag means Rojo will treat the output as a **Script**.

```clpp
#include <clpp/roblox.clh>

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

1. `#include <clpp/roblox.clh>` — IntelliSense for engine types. No Luau is emitted for this header.
2. `struct HelloServer` + `void HelloServer::Greet` — define methods with `::`. Call them with `.` (`hello.Greet(player)`).
3. `player.Name` — **property** (dot).
4. `"Player name: " .: player.Name` — **concatenation** (`.:` → Luau `..`).
5. `void init()` — runs at the end of Scripts and LocalScripts.
6. `GetService<Players>()` — typed service lookup.
7. `for (Player player in players.GetPlayers())` — range-for. `in` is the collection.
8. `players.PlayerAdded~>Connect` — `.` reads the signal, `~>` gives Connect to Janitor. Manual (no janitor) is `::Connect`.
9. `func (Player playerEntered) { ... }` — anonymous callback. Luau closures still see outer locals. There is no C++ capture list `[]`.

Compile:

```bash
clpp compile hello.server.clpp
```

You get Luau with `function HelloServer:Greet`, `game:GetService("Players")`, and an `init()` call at the bottom.

Next: [Mental model](mental-model).
