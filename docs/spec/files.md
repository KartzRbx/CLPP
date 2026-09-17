# `.clh` / `.clp` / `.clpp` files

Everything the compiler reads is CL++. The extension only separates **role**.

| Extension | Role | C++ analog | Emitted? |
| --- | --- | --- | --- |
| `.clh` | Header: `struct`, constants, prototypes | `.h` / `.hpp` | Types (`export type`) and constants; no bodies |
| `.clp` | Implementation (modules) | `.c` / `.cpp` without a tag | Yes — ModuleScript by default |
| `.clpp` | CL++ implementation (scripts and methods) | `.cpp` | Yes — Script / LocalScript / ModuleScript from the tag |

`.clp` and `.clpp` accept the same tags. Prefer `.clh` + `.clpp` in C++ style (declare in the header, define in the script).

## Filename tags

The **key in the filename** chooses the Roblox instance (same idea as roblox-ts / Rojo).

| Source | Instance | RunContext |
| --- | --- | --- |
| `LeaderstatsServer.server.clpp` | **Script** | Server |
| `Hud.client.clpp` | **LocalScript** | Client |
| `Tools.plugin.clpp` | Script | Plugin |
| `Boot.legacy.clpp` | Script | Legacy |
| `Boot.legacy.server.clpp` | Script | Legacy |
| `Boot.legacy.client.clpp` | LocalScript | Legacy |
| `Damage.clp` / `Damage.clpp` (no tag) | **ModuleScript** | — |
| `LeaderstatsServer.clh` | type ModuleScript | — |

One input file, one output file:

```
src/server/leaderstats/LeaderstatsServer.server.clpp  →  out/server/leaderstats/LeaderstatsServer.server.luau
src/client/hud/Hud.client.clpp                        →  out/client/hud/Hud.client.luau
src/shared/config.clp                                 →  out/shared/Config.luau
src/shared/PlayerData.clh                             →  out/shared/PlayerData.luau
```

## Includes

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
#include "LeaderstatsServer.clh"
#include "../shared/PlayerData.clh"
```

| Form | Effect |
| --- | --- |
| `#include <clpp/roblox.clh>` | IntelliSense only. No Luau emit |
| `#include <clpp/generated/instances.clh>` | IntelliSense for Roblox classes |
| `#include <clpp/datatypes.clh>` | IntelliSense for `Vector3`, `string_concat`, … |
| `#include <clpp/libs/janitor.clh>` | IntelliSense **and** a `require` of the lib |
| `#include "Stem.clh"` with the same stem as the `.clpp` | **Inlined** (class body) |
| `#include "Other.clh"` / `"Other.clp"` | `require` (Rojo path) |
| `#pragma once` | Include guard; stripped before parse |
| `#pragma strict` | Emits `--!strict` |
| `#pragma nostrict` | Emits `--!nonstrict` (`nstrict` / `nonstrict` aliases) |
| `#pragma native` | Emits `--!native` |
| `#pragma optimize` / `#pragma optimize 2` | Emits `--!optimize 2` |

`using …;` is skipped. `namespace { }` is flattened. `enum` / `template` / `typedef` / `extern` declarations are skipped. `//` and `/* */` comments are stripped.

## `void init()`

If the file defines `void init()`, emit places `init()` at the end of Scripts and LocalScripts. There is no `int main()`.

Shared modules **must not** define `init()` unless they should run on `require`.
