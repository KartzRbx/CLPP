---
title: Roblox instances and services
---

# Roblox instances and services

## new

```clpp
auto* coins = new IntValue(leaderstats);
auto* janitor = new Janitor();
```

| Construct | Emit |
| --- | --- |
| `new Folder(player)` | `Instance.new("Folder"); folder.Parent = player` |
| `new Janitor()` | `Janitor.new()` |
| `Vector3(8, 1, 8)` | `Vector3.new(8, 1, 8)` |

## GetService

```clpp
Players* players = GetService<Players>();
RunService* run = GetService<RunService>();
```

## Events

```clpp
players::PlayerAdded~>Connect(func [](Player* player) {
    post(player.Name);
});

player::GetPropertyChangedSignal("Name")~>Connect(func []() {
    post("renamed");
});
```

`~>` registers the connection on `janitor`, `self.janitor`, or a synthetic `__janitor` (with `BindToClose` inside `init()`).

## Libraries

`#include <clpp/libs/janitor.clh>` both documents Janitor and emits `require`. DataService tables use `DataService:Server` / `DataService:Client`.

Next: [Observable and signals](observables-signals).
