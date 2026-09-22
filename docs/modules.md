---
title: Modules and imports
description: Named import { } from is the language module surface. Headers and angle includes are host/legacy.
---

# Modules and imports

CL++ modules are **files**. The language surface for pulling another file’s symbols is:

```clpp
import { Wallet } from "./PlayerData.clh";
import { PlayerData as Data } from "./PlayerData.clh";
import { Wallet, PlayerData } from "./PlayerData.clh";
```

This is the form new code should use. See [RFC 0003](https://github.com/KartzRbx/CLPP/blob/main/rfc/0003-module-system.md).

## Who is visible?

Top-level `struct` / `class` / `interface` / `enum` / `type` / `using` / free functions in the module file are **exports**. Named import merges only the listed names (plus their fields/methods). There is no separate `export` keyword in the MVP — visibility is “top-level in the module file.”

## How does the compiler resolve paths?

| Form | Resolution |
| --- | --- |
| `"./Foo.clh"` / `"../shared/Bar.clp"` | Relative to the importing file |
| Same stem as the current `.clpp` (legacy `#include`) | Text splice for header/impl pairs |
| Angle `<clpp/…>` | **Cluaupp / platform** prelude — not a language module |

Missing module path → diagnostic **CLPP0801**.

## Cycles

The module graph keeps a `seen` set and runs **`detect_cycles`**. A real cycle produces diagnostic **CLPP1001** (break it with a shared types-only `.clh` or by removing the back-edge). The `seen` set also prevents infinite recursion while loading.

## Type-only / star / package (status)

| Feature | Status |
| --- | --- |
| `import { X } from "…"` | **Implemented** |
| `import { X as Y }` | **Implemented** |
| `import type { … }` | Not yet — planned on the same resolver |
| `import * as M` | Not yet |
| Package / registry resolution | Host concern (Cluaupp / Rojo layout) |

## Emit

Named imports become `require(…)` entries on the compile artifact (`CompileContext.requires`). Cluaupp / Rojo map those paths into the DataModel.

## Legacy `#include`

```clpp
#include "PlayerData.clh"      // different stem → still require()
#include "Main.clh"            // same stem as Main.clpp → splice
#include <clpp/roblox.clh>     // platform IntelliSense — Cluaupp
```

Prefer `import { … } from` for **language** dependencies. Keep angle includes for **engine / generated** headers owned by Cluaupp. Details: [reference/import](reference/import), legacy notes on [reference/include](reference/include).

## File roles (unchanged)

| Extension | Role | Typical Roblox instance |
| --- | --- | --- |
| `.clh` | Types, constants, prototypes | ModuleScript (types) |
| `.clp` | Shared module | ModuleScript |
| `.clpp` | Script / methods | Script / LocalScript / ModuleScript from tag |

Next: [Types](types) · [Files and tags](files).
