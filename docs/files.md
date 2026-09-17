---
title: Files, tags, includes
---

# Files, tags, includes

Everything the compiler reads is CL++. The extension only chooses **role**.

| Extension | Role | C++ analog | Emitted? |
| --- | --- | --- | --- |
| `.clh` | Header: `struct`, constants, prototypes | `.h` | Types and constants; no function bodies |
| `.clp` | Module implementation | `.c` / untagged `.cpp` | ModuleScript by default |
| `.clpp` | Script / methods | `.cpp` | Script / LocalScript / ModuleScript from the tag |

Prefer `.clh` + `.clpp`: declare in the header, define in the script.

## Filename tags

| Source | Instance |
| --- | --- |
| `Foo.server.clpp` | Script |
| `Foo.client.clpp` | LocalScript |
| `Foo.plugin.clpp` | Plugin Script |
| `Foo.legacy.clpp` | Legacy Script |
| `Foo.clp` / `Foo.clpp` (no tag) | ModuleScript |
| `Foo.clh` | type ModuleScript |

## Includes

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
#include "LeaderstatsServer.clh"
#include "../shared/PlayerData.clh"
```

| Form | Effect |
| --- | --- |
| `<clpp/roblox.clh>` | IntelliSense only |
| `<clpp/libs/janitor.clh>` | IntelliSense **and** `require` |
| `"Stem.clh"` with the same stem | **Inlined** into the `.clpp` |
| `"Other.clh"` | `require` |

`#pragma once` is valid in `.clh` (include guard). `#pragma strict` → `--!strict`. `#pragma nostrict` → `--!nonstrict`. `#pragma native` → `--!native`. `#pragma optimize` / `#pragma optimize 2` → `--!optimize 2`.

`using …;` is skipped. `namespace { }` is flattened. `//` and `/* */` comments are stripped.

Next: [Types and values](types).
