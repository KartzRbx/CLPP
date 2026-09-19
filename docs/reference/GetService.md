---
title: "GetService"
sidebar_label: "GetService"
---

# GetService

<div class="clpp-ref-meta">Builtin · Roblox</div>

Looks up a Roblox service by type name. The only generic besides collections and `static_cast`.

## Syntax

```clpp
T GetService<T>();
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `class name` | Service class, e.g. `Players`, `RunService`. |

## Return value

An Instance of class `T` (`game:GetService("T")`).

## Luau emit

`game:GetService("T")`

## Description

`T` is the Roblox class name, not a C++ template you define. You must `#include <clpp/roblox.clh>` (IntelliSense) so the editor knows the type.

There is no `game:GetService` in CL++ source — always this form.

## Example

```clpp
#include <clpp/roblox.clh>

void init() {
    Players players = GetService<Players>();
    post("online: " .: players.GetPlayers());
}
```

Emits:

```luau
local players: Players = game:GetService("Players")
print("online: " .. players:GetPlayers())
```

## See also

[new](new) · [include](include) · [instance-pointer](instance-pointer)
