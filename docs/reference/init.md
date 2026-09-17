---
title: "void init()"
sidebar_label: "init"
---

# void init()

<div class="clpp-ref-meta">Script entry</div>

Script / LocalScript entry point. CL++ has no `int main()`.

## Syntax

```clpp
void init() {
    // ...
}
```

## Parameters

None.

## Return value

`void`.

## Luau emit

`init()  (called at the end of the file)`

## Description

Only Scripts and LocalScripts run `init()` at the bottom of the emitted file. Untagged `.clpp` files are ModuleScripts: they `return` the table of `Class::` methods and must **not** rely on `init` as a Roblox entry.

Construct **one** service object in `init()` and close over it from lambdas — that is the game singleton.

In `init()`, a synthetic `__janitor` is created and also `game:BindToClose`.

## Example

```clpp
#include <clpp/roblox.clh>

void init() {
    Players* players = GetService<Players>();
    post("ready");
}
```

Emits:

```luau
local players: Players = game:GetService("Players")
print("ready")
init()
```

## See also

[struct](struct) · [lambda](lambda) · [operator-janitor](operator-janitor)
