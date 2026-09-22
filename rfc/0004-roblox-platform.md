# RFC 0004 — roblox-platform

Status: Accepted (MVP)

## Problem

Roblox Instance types were mixed into compiler construction (`Session::new()` always loaded them), so CL++ looked like a Roblox-only language.

## Syntax

No extra grammar for services. `GetService<T>()` remains a call form; `new Folder(...)` is `new`. Member names (`FindFirstChild`, `Name`) come from the prelude, not from special AST kinds.

## Semantics

- `Session::language()` — core types only.
- `Session::with_roblox_platform()` / `Session::new()` — Cluaupp host: `platform::load_prelude` binds generated Instance headers plus service structs (`Players`, …).
- `GetService<T>()` types as nominal `T` **if** that struct exists in the platform database.
- `FindFirstChild<T>(name)` types as `optional<T>` (narrowing via guard / `if`).

## AST

AST does not embed the Roblox API dump. Platform metadata is versioned generated `.clh` under `stdlib/clpp/generated/`.

## Binder / Symbols

Prelude is a `BoundFile` cached per process. Consumer files do not re-parse the dump.

## Types

Instance hierarchy is IS-A (`struct TextLabel : Instance`).

## Diagnostics

Library types without their header still use `CLPP0605` for ClppLibs. Unknown members on Instance use `CLPP0604`.

## LSP

Default `clpp lsp` session uses the Roblox platform so Studio scripts complete. Non-Roblox hosts should construct `Session::language()`.

## Codegen

Emitter may still emit `game:GetService("…")` for the call form. Constructors/PlayerEntered remain Roblox-aware in the emitter, not in the parser.

## Tests

`language_session_has_no_roblox_prelude`, `getservice_and_new_have_nominal_types`.

## Documentation

`docs/architecture/ROBLOX_PLATFORM.md`, `CLUAUPP_ARCHITECTURE.md`.
