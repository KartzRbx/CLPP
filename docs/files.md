---
title: Files, tags, modules
description: Extensions, Rojo tags, and how modules connect with link.
---

# Files, tags, modules

Everything the compiler reads is CL++. The extension chooses the **role**. **`link`** chooses the **dependency**.

## Extensions

| Extension | Role | Emitted? |
| --- | --- | --- |
| `.clh` | Types, constants, prototypes | Types / constants; no function bodies |
| `.clp` | Shared module | ModuleScript by default |
| `.clpp` | Script / methods | Script / LocalScript / ModuleScript from the tag |

## Filename tags

| Source | Instance |
| --- | --- |
| `Foo.server.clpp` | Script |
| `Foo.client.clpp` | LocalScript |
| `Foo.plugin.clpp` | Plugin Script |
| `Foo.legacy.clpp` | Legacy Script |
| `Foo.clp` / `Foo.clpp` (no tag) | ModuleScript |
| `Foo.clh` | type ModuleScript |

## Modules

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link "./PlayerData.clh" as PlayerData;
link "./PlayerData.clh" as Data;
```

Full rules: [Modules and links](modules).

`#pragma once`, `#pragma strict`, `#pragma native`, and `#pragma optimize` are valid. `#include` is not.

Next: [Modules](modules) · [Types](types).
