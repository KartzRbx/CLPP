---
title: Structs and methods
---

# Structs and methods

## Types module

```clpp
#pragma once

struct LeaderstatsServer {
    static constexpr int STARTING_COINS = 0;
    void PlayerEntered(Player player);
};
```

Consumers pull it with:

```clpp
link "./LeaderstatsServer.clh" as LeaderstatsServer;
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

void init() {
    LeaderstatsServer server;
}
```

`Class::Method` emits `function Class:Method(...)`. Inside, [`@this`](reference/this) is `self`. `@janitor` is `self.janitor`.

:::tip[Receiver]
`@` is the sigil. `@this` is the object. `@field` is `self.field`. Calls from outside still use `.` (`hello.Greet(player)`).
:::

## OOP model

CL++ OOP is **struct/class/interface + methods + optional parent**, emitting Luau tables — not a full C++ object model (no language-level vtables/destructors). Inheritance shares members; `public` / `private` are checked. See [TYPE_SYSTEM](architecture/TYPE_SYSTEM).
