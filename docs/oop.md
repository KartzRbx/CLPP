---
title: Structs and methods
---

# Structs and methods

## Header

```clpp
#pragma once
#include <clpp/libs/janitor.clh>

struct LeaderstatsServer {
    static constexpr int STARTING_COINS = 0;
    Janitor janitor;
    void PlayerEntered(Player player);
};
```

## Implementation

```clpp
void LeaderstatsServer::PlayerEntered(Player player) {
    guard (player != null) else {
        warn("Invalid player");
        return;
    }
    Folder folder = new Folder(player);
    folder.Name = "leaderstats";
}
```

`Class::Method` emits `function Class:Method(...)`. Inside, [`@this`](reference/this) is `self`. `@janitor` is `self.janitor`. Bare fields still become `self.field`. `this` without `@` is the same alias.

## init singleton

```clpp
void init() {
    Players players = GetService<Players>();
    LeaderstatsServer leaderstatsServer;

    for (Player player in players.GetPlayers()) {
        leaderstatsServer.PlayerEntered(player);
    }

    players.PlayerAdded~>Connect(func (Player playerEntered) {
        leaderstatsServer.PlayerEntered(playerEntered);
    });
}
```

Build **one** service object and capture it. That is the game's singleton.

Field-only structs in a `.clh` (PlayerData templates) emit `const function Name()` with default fields.

Untagged files that only define `Class::` methods `return` the table (ModuleScript).

Next: [I/O](io).
