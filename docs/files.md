---
title: Files, tags, modules
description: Extensions, Rojo tags, and how modules connect — import first, include only for host/legacy.
---

# Files, tags, modules

Everything the compiler reads is CL++. The extension chooses **role**; **`import`** chooses **dependencies**.

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

## Modules (canonical)

```clpp
import { Wallet } from "./PlayerData.clh";
import { PlayerData as Data } from "./PlayerData.clh";
```

Full rules: [Modules and imports](modules).

## Platform / legacy includes

Angle brackets are **Cluaupp / platform** (IntelliSense and lib `require`), not the language module system:

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
```

Same-stem quoted `#include "Foo.clh"` next to `Foo.clpp` remains a **header/impl splice** for older layouts. Prefer `import` for cross-file language symbols.

`#pragma once` · `#pragma strict` / `nostrict` · `#pragma native` · `#pragma optimize` are valid.

Next: [Modules](modules) · [Types](types).
