---
title: Modules
description: Organizing source files with link, type and function boundaries, directives, and no headers.
---

# Modules

CL++ modules execute on its own VM. See [compiler status](../reference/status.md) for the remaining language work.

Split entities, combat, inventory, and maps into collaborating source modules. `link` connects those sources.

## One source module, no header

CL++ has no header files and does not split prototypes and implementations into separate declaration files. A module holds data definitions and complete functions. Its API is a boundary between operations callers need and internal details; that boundary is not chosen by a header extension.

| Source file | Responsibility |
| --- | --- |
| `Entities.clp` | Player and target data |
| `Combat.clp` | Damage rules |
| `Game.clp` | Entry and coordination |

Use `link`; `#include` is not part of this model.

## Linking source files

```clp
link "./Entities.clp" as Entities;
link "./Combat.clp" as Combat;
auto player = Entities.Player { Name = "Ada" };
post(player.Name);
post(Combat.Damage(player.Strength, player.Defense));
```

```output
Ada
7
```

The string names a file and `as` provides an alias. Module functions can be called as `Combat.Damage(...)`. Relative paths resolve from the file containing the link, including links inside imported modules.

### Crossing the API boundary

Modules expose declarations with `export` before the declaration: `export def Player`, `export number Damage(...)`, and `export const number INITIAL_HEALTH = 100;`. Declarations without `export` are internal. Callers use the alias to access the public API.

Qualified types have distinct module identities. The compiler enforces exports and keeps declarations without them internal. There is no header-based public/private rule or separate `import type` form.

## Types and constants

```clp file=Entities.clp
<< Entities.clp
export def Player {
    string Name;
    number Strength = 10;
    number Defense = 3;
}
export const number INITIAL_HEALTH = 100;
```

`def` covers data shapes, aliases, unions, callback types, and generics. See [Definitions and types](./07-def-e-tipos.md). Keep the source API small without introducing a separate declaration file.

## A complete module boundary

```text
Entities.clp  shared data
Combat.clp    combat functions
Game.clp      program coordination
```

```clp file=Combat.clp
<< Combat.clp
export number Damage(number strength, number defense) {
    return strength > defense ? strength - defense : 1;
}
```

```clp
<< Game.clp
link "./Combat.clp" as Combat;
post(Combat.Damage(20, 5)); << 15
```

The source module explicitly exports the typed function. Shared definitions and constants cross the boundary only when exported.

### Conflicts and cycles

If dependencies offer the same name, the intended checker should locate both declarations. Aliases distinguish module function names; this does not prove that target type-import conflicts are already handled. Proposed privacy or conflict diagnostics are not current guarantees.

Avoid A importing B while B imports A. Extract shared data into a third module. Cyclic links are rejected. Imported modules may hold private mutable state and run setup statements. Dependencies initialize first, once per normalized path per program run; repeated aliases share state. Initialization errors stop the entry program, and reads before a binding initializes raise an error. Concurrent writes require explicit synchronization.

## A transpiler application written in CL++

CL++ officially runs through its own VM. A user could write a transpiler **in CL++** that converts another source representation to Luau modules. Such an application's output might look like this:

```lua
-- Illustrative output of a user-written application.
local Entities = require(script.Parent.Entities)
type Player = Entities.Player
```

Path mapping, visible types, Roblox ModuleScript references, and generated declarations belong to that application's contract and tests. This is not an official CL++ Luau backend, promised export transformation, or validated Roblox execution path.

## Pragmas and compilation modes

Pragmas describe compilation settings and belong at the start of a file. The parser implements strict and nostrict; optimization remains a separate proposed directive.

| Source proposal | Intended meaning | Status |
| --- | --- | --- |
| `pragma strict;` | Strict checking | Implemented; rejects missing type evidence |
| `pragma nostrict;` | Gradual checking during migration | Implemented default mode |
| `pragma optimize;` | Request optimization | Behavior needs definition and validation |

```clp
pragma strict;
number score = 42;
post(score);
```

```output
42
```

The source manual's `pragma native;` concerned Luau native compilation. It is not a setting for an official CL++ Luau backend; a user-written transpiler could separately define such an application option. Its former header-oriented `pragma once` has no header role because CL++ has no headers. Dependency resolution and cycle prevention belong to the module system.

## Context annotations

The supplied examples include `@server`, `@client`, and `@parallel` for application authority or execution context. Their checking and allowed APIs need a contract; they do not make Roblox services part of the official VM. See [Contexts and safety](./10-contextos-e-seguranca.md). Proposed diagnostic identifiers are not implementation evidence.

## Module design

- Keep each module's API small and give it one clear responsibility.
- Separate caller-facing operations from internal details without a header file.
- Extract shared definitions when dependencies form a cycle.
- Name modules after domains: `Combat`, `Inventory`, `Map`.
- Resolve each relative link from the source file that contains it.
