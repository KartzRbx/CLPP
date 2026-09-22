# RFC 0001 — language-model

Status: Accepted (MVP)

## Problem

CL++ was at risk of becoming “C++ syntax with ad-hoc tables” or a TypeScript clone. Symbols lived on the AST, Roblox leaked into the grammar, and the editor recompiled as a batch job.

## Syntax

The source surface stays C++-like: `struct`, methods, `@`, `~>`, `.:`, headers/impl, `#include`, `import { Name } from "path"`. Pest remains the only lexer. Templates, macros, pointers, and `=>` from the spec example stay out.

## Semantics

Pipeline: Pest → AST → Binder (`SymbolId`) → interned `TypeId` → checker / narrowing → Semantic DB.

Inheritance is IS-A (base walk), not intersection. `self` is a bound symbol on instance methods.

## AST

`src/ast` is the language. Platform names (`Players`, `Instance`) are not AST node kinds. `GetService<T>()` is a call form whose type is filled from the platform database.

## Binder / Symbols

`src/binder` + `src/symbols`. Declarations, fields, methods, aliases, enums, and `self` are symbols. The binder does not type-check.

## Types

`src/types`: interned `TypeId`, `Unknown` / `Error`/`Any` / `Never`, optional = `Union(T, Nil)`.

## Diagnostics

`src/diagnostics` re-exports stable `CLPP####` codes. Syntax recovery (Pest + `parse_for_ide`) is separate from semantic diagnostics.

## LSP

`src/lsp` + `src/analysis` consume `CompilerApi`. They do not run the Luau emitter.

## Codegen

`src/emitter` / `src/codegen` lower to Luau after a successful check. The driver (`src/driver`) owns when emit runs.

## Tests

`tests/core.rs` covers binder, aliases, narrowing, session queries, and modules.

## Documentation

`docs/architecture/LANGUAGE_DESIGN.md`, `COMPILER_ARCHITECTURE.md`.
