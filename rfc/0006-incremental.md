# RFC 0006 — incremental

Status: **Deferred (after MVP)**

See [docs/architecture/DEFERRED.md](../docs/architecture/DEFERRED.md).

## Problem

Full re-bind/re-check per keystroke will not scale. rustc-style queries, HIR, arenas, and incremental parse are the right model — after the semantic core is stable.

## Syntax

No syntax change.

## Semantics

Not implemented. `Session` currently hashes the file and rebuilds that file when the hash changes. Module `seen` sets prevent cyclic re-entry. That is a cache, not a query engine.

## AST

No incremental parser / green tree. Pest reparses the expanded source.

## Binder / Symbols

No query keys. Binder is a full pass.

## Types

`TypeDatabase` is interned but not incrementally invalidated.

## Diagnostics

Recomputed per `check_source`.

## LSP

Debounce exists in `lsp/server.rs`; it is not a salsa/query graph.

## Codegen

No incremental emit.

## Tests

None required until this RFC is accepted for implementation.

## Documentation

Roadmap phases 11 and 13; points 065–067, 108–112, 213–215, 241–258.
