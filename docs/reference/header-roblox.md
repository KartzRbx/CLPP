---
title: "<clpp/roblox.clh>"
sidebar_label: "roblox.clh"
---

# `<clpp/roblox.clh>`

<div class="clpp-ref-meta">Standard header</div>

Engine globals for IntelliSense. Does not emit `require`.

## Syntax

```clpp
#include <clpp/roblox.clh>
```

## Parameters

None.

## Return value

None.

## Luau emit

`(no require)`

## Description

Gives the editor `game`, `workspace`, services, and the usual Roblox globals. Runtime wiring of generated Instance classes is [Cluaupp](https://github.com/KartzRbx/Cluaupp).

## Example

```clpp
#include <clpp/roblox.clh>
void init() {
    Players* players = GetService<Players>();
}
```

Emits:

```luau
local players: Players = game:GetService("Players")
```

## See also

[GetService](GetService) · [include](include) · [header-instances](header-instances)
