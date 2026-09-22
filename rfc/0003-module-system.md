# RFC 0003 — module-system

Status: Accepted (MVP)

## Problem

`.clh` was often spliced as text. Different-stem quoted includes needed a symbol graph. Named `import {}` is the language surface for modules.

## Syntax (canonical)

```clpp
import { Wallet } from "./PlayerData.clh";
import { PlayerData as Data } from "./PlayerData.clh";
import { Wallet, PlayerData } from "./PlayerData.clh";
```

Prefer this in all new CL++ sources.

There is **no `export` keyword**. Every top-level struct / function / type alias / enum in a module file is importable (see `merge_exports` in `src/modules`). Marking privacy is `private:` inside structs, not file-level `export`.

### Legacy / host

```clpp
#include "PlayerData.clh"   // different stem → still require() (Cluaupp / old code)
#include "Main.clh"         // same stem as Main.clpp → text splice (header/impl pair)
#include <clpp/roblox.clh>  // Cluaupp platform prelude — not a language module
```

## Semantics

`src/modules` binds the dependency, runs checker resolve into the shared `TypeDatabase`, and merges export symbols. Named import merges only listed names (plus their fields/methods). `as` renames the top-level symbol in the consumer scope. Cycles are cut with a `seen` set.

## AST

`Item::Import { names: Vec<ImportName>, module, line, span }` where `ImportName { name, alias }`.

## Binder / Symbols

Imported structs/functions/aliases appear in the consumer file scope after session merge (local alias when present).

## Types

Child `checker::resolve` registers nominal types in the session type database so `Wallet w; w.` completes.

## Diagnostics

Missing named import path: `CLPP0801`.
Import cycle: `CLPP1001`.

## LSP

Completion/hover use `Session::check_source`, which runs the module graph.

## Codegen

Named imports become `require(...)` via `CompileContext.requires` (Rojo-style path). CLI / Cluaupp can map paths when they ship the host layout.

## Tests

`named_import_imports_exported_symbols`, `parses_named_import`, `import_alias_binds_local_name`.

## Documentation

This RFC; `src/modules/mod.rs`; `docs/architecture/DONT_REINVENT.md`.
