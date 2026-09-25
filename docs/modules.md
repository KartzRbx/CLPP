---
title: Modules and links
description: link is the only module form. import and #include are rejected.
---

# Modules and links

Pull another file or a host namespace with `link`:

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link @game.ReplicatedStorage.Modules.Combat as CombatModule;
link "./PlayerData.clh" as PlayerData;
link "./PlayerData.clh" as Data;
```

`import { … } from` and `#include` are syntax errors. The diagnostic is `` use `link` ``.

## Who is visible?

Top-level `struct`, `class`, `interface`, `enum`, `type`, `using`, and free functions in the linked file are the module's symbols. `as` is the name in this file. There is no `export` keyword.

## How paths resolve

| Form | Resolution |
| --- | --- |
| `@clpp.libs.janitor` | Standard library (`CLPP_INCLUDE`, the install pack, or `~/.clpp/include`) |
| `@game.ReplicatedStorage.Modules.Combat` | Rojo `default.project.json`, or the place named in `clpp.toml` |
| `"./Foo.clh"` / `"../shared/Bar.clp"` | Relative to this file |

Missing path → **CLPP0801**. A cycle → **CLPP1001**.

## Emit

| Form | Luau |
| --- | --- |
| `link @game.Service.Path as Name` | `game:GetService("Service")` and `require(Service.Path)` |
| `link @clpp…` | Prelude comment only. Not a game `require`. |
| `link "./Path" as Name` | `require` of that module |

Luau is emitted only when Context Safety (`CLUAU_AUTH`) and parallel safety (`CLUAU_PAR`) pass.

## File roles

| Extension | Role | Typical Roblox instance |
| --- | --- | --- |
| `.clh` | Types, constants, prototypes | ModuleScript |
| `.clp` | Shared module | ModuleScript |
| `.clpp` | Script or methods | Script, LocalScript, or ModuleScript from the tag |
