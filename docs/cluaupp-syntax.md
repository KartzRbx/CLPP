---
title: CL++ syntax for Cluaupp
description: The current CL++ surface and more than 30 scripts to retarget Cluaupp templates and tests.
---

# CL++ syntax for Cluaupp

Language law for host templates. Modules are `link` only. `import { } from` and `#include` are parse errors (`use link`).

File tags set `RunContext` for the checker: `*.client.clpp` is Client, `*.server.clpp` is Server, anything else is Module.

| Write | Meaning | Luau |
| --- | --- | --- |
| `link @clpp.libs.janitor as Janitor` | Standard-library prelude | Comment, no `require` |
| `link @game.ReplicatedStorage.Modules.Combat as Combat` | DataModel module | `GetService` + `require` |
| `link "./Health.clh" as Health` | Project file | `require` |
| `player.Name` / `player.Kick()` | Property or instance method | `.` / `:` |
| `age: int` / `player:Kick()` | Type, or protected call | `:` / `pcall` |
| `Class::Method` / `task::wait` | Method definition or static | `:` / `.` |
| `"hi " .: name` | Concat | `..` |
| `signal~>Connect(fn)` | Sweep-owned connect | `sweep:Add(..., "Disconnect")` |
| `items[0]` | Index only | `[key]` |
| `@this` / `@field` | Receiver inside `Class::Method` | `self` / `self.field` |
| `@server` / `@client` / `@parallel` | Authority or parallel region | Checker: `CLUAU_AUTH001`, `CLUAU_PAR001` |

There is no `int main()`. Scripts start at `void init()`. No `->`, no pointers, no `export`.

## 1. Prelude and a server entry

```clpp
link @clpp.roblox;

void init() {
    Players players = GetService<Players>();
    post("online");
}
```

## 2. Alias a library

```clpp
link @clpp.libs.janitor as Janitor;

void init() {
    Janitor janitor;
    janitor.Add(1);
    janitor.Cleanup();
}
```

## 3. DataModel module

```clpp
link @game.ReplicatedStorage.Modules.Combat as CombatModule;

void init() {
    CombatModule combat;
    combat.Start();
}
```

## 4. Relative header

```clpp
link "./PlayerData.clh" as PlayerData;

void init() {
    PlayerData data;
    data.Coins = 0;
}
```

## 5. Relative alias

```clpp
link "../Shared/PlayerData.clh" as Data;

void init() {
    Data data;
    post(data.Money);
}
```

## 6. Several links

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link @game.ReplicatedStorage.Shared.Net as Net;
link "./Wallet.clh" as Wallet;
```

## 7. Header struct

```clpp
#pragma once

struct Wallet {
    int Coins = 0;
    int Gems = 0;
};
```

## 8. Script calls a method

```clpp
link "./HelloServer.clh" as HelloServer;

void init() {
    HelloServer hello;
    hello.Greet();
}
```

## 9. Method definition

```clpp
struct HelloServer {
    void Greet(Player player);
};

void HelloServer::Greet(Player player) {
    post("hello, " .: player.Name);
}
```

## 10. Receiver fields

```clpp
struct Bank {
    int Coins;
    void Deposit(int amount);
};

void Bank::Deposit(int amount) {
    @Coins = @Coins + amount;
    post(@this);
}
```

## 11. Player added

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;

void init() {
    Players players = GetService<Players>();
    Janitor janitor;
    players.PlayerAdded~>Connect(func (Player player) {
        post(player.Name);
    });
}
```

## 12. Manual connect

```clpp
link @clpp.roblox;

void init() {
    Players players = GetService<Players>();
    players.PlayerAdded::Connect(func (Player player) {
        post(player.UserId);
    });
}
```

## 13. Guard

```clpp
void Greet(Player player) {
    guard (player != null) else {
        warn("missing player");
        return;
    }
    post(player.Name);
}
```

## 14. If and else

```clpp
void Pay(int coins) {
    if (coins > 50) {
        post("enough");
    } else if (coins == 0) {
        warn("none");
    } else {
        post(coins);
    }
}
```

## 15. Loops

```clpp
void Count(Players players) {
    int n = 3;
    while (n > 0) {
        n = n - 1;
    }
    for (int i = 0; i < 10; i = i + 1) {
        post(i);
    }
    for (Player player in players.GetPlayers()) {
        post(player.Name);
    }
}
```

## 16. Match on instances

```clpp
void Touch(Instance instance) {
    match (instance) {
        Part p => { p.Anchored = true; },
        Model m => { post(m.Name); },
        _ => { warn("skip"); },
    }
}
```

## 17. Switch

```clpp
void Label(int n) {
    switch (n) {
        case 1:
            post("one");
            break;
        default:
            break;
    }
}
```

## 18. Option

```clpp
void Show() {
    optional<int> maybe = Some(1);
    match (maybe) {
        Some v => { post(v); },
        None => { },
    }
}
```

## 19. Result and try

```clpp
Result<int, string> parse(string s) {
    if (s == "") { return Err("empty"); }
    return Ok(to_number(s));
}

int use(string s) {
    int n = parse(s)?;
    return n + 1;
}
```

## 20. Result match

```clpp
int unwrap_or(Result<int, string> r, int fallback) {
    match (r) {
        Ok v => { return v; },
        Err e => { return fallback; },
    }
}
```

## 21. Collections

```clpp
void Bags() {
    array<string> names = { "Kartz", "Player1" };
    dictionary<string, int> stats = {
        { "Coins", 100 },
        { "Gems", 50 }
    };
    post(stats.Coins);
    post(names[0]);
}
```

## 22. Designated init

```clpp
struct DataServiceOptions {
    string StoreName;
    bool UseMock;
};

void Boot() {
    DataServiceOptions options = {
        .StoreName = "PlayerData",
        .UseMock = true,
    };
}
```

## 23. Signal

```clpp
signal<Player, int> OnCoins;

void Watch(Player player) {
    OnCoins~>Connect(func (Player who, int amount) {
        post(who.Name .: ": " .: amount);
    });
    OnCoins.Fire(player, 500);
}
```

## 24. Observable

```clpp
void Hud() {
    observable int coins = 100;
    coins.OnChange(func (int next) {
        post(next);
    });
    coins = 50;
}
```

## 25. Template string and concat

```clpp
void Line(Player player) {
    post(`PlayerName is {player.Name}`);
    post("hello, " .: player.Name);
}
```

## 26. Protected call

```clpp
void SafeKick(Player player) {
    player:Kick();
}
```

## 27. pcall destructure

```clpp
void Probe() {
    auto [success, result] = pcall(func () {
        return 1;
    });
    post(success);
}
```

## 28. Spawn

```clpp
void Later() {
    spawn {
        task::wait(2);
        post("done");
    };
}
```

## 29. Parallel block

```clpp
void Tick() {
    parallel {
        post("desync");
    };
}
```

## 30. Server-only function

```clpp
@server
void Save();

void init() {
    Save();
}
```

A Client file that contains this is `CLUAU_AUTH001`. The same file as `*.server.clpp` is allowed. No Luau is emitted when AUTH or PAR fails.

## 31. Client-only function

```clpp
@client
void OpenMenu();

void init() {
    OpenMenu();
}
```

Calling `OpenMenu` from a Server file is `CLUAU_AUTH001`.

## 32. Parallel mutation is rejected

```clpp
@parallel
void tick() {
    coins = 1;
}
```

That assignment is `CLUAU_PAR001`.

## 33. Pragmas

```clpp
#pragma once
#pragma strict

struct Marker {
    int Value = 1;
};
```

`#pragma native` and `#pragma optimize` are also legal. `#include` is not.

## 34. New instance

```clpp
void FolderFor(Player player) {
    Folder folder = new Folder(player);
    folder.Name = "leaderstats";
}
```

## 35. Const and auto

```clpp
struct Limits {
    static constexpr int START = 0;
};

void Read(string raw) {
    auto n = to_number(raw);
    const int cap = 99;
    post(n);
    post(cap);
}
```

## 36. Bind to close

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;

void init() {
    Janitor janitor;
    game.BindToClose(func () {
        janitor.Cleanup();
    });
}
```

## What Cluaupp should stop emitting

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
import { Wallet } from "./PlayerData.clh";
```

Replace those with `link` from sections 1–6. Keep `GetService<Players>()` as a generic call. The host still owns the service registry (`CLUAU_SVC001` if you check unknown services before compile).
