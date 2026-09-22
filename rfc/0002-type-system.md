# RFC 0002 — type-system

Status: Accepted (MVP)

## Problem

Types were strings. Subtyping, inheritance, and optional narrowing could not share one identity.

## Syntax

`int` / `float` / `bool` / `string` / `void`, `optional<T>`, `T | U`, `array<T>`, `map<K, V>`, tuples, `using` / `type` aliases, `interface`, `enum`, `as` / `static_cast`. Simple generics on aliases and a few calls (`FindFirstChild<T>`). No C++ templates.

## Semantics

Every type is a `TypeId` interned in `TypeDatabase`. `Unknown` is not `Any`. `Never` is bottom. Subtyping:

- Nominal IS-A walks `StructInfo.bases` (diamond-safe).
- Intersection is a separate kind; it is not inheritance.
- Optional is `Union(T, Nil)`. Guards and `if (x)` narrow it.

## AST

Type syntax is parsed to strings on decls; the checker intern them. The AST does not store `TypeId`.

## Binder / Symbols

Symbols hold `declared_type: Option<String>` and `type_id` filled by `checker::resolve`.

## Types

See `src/types`. Members live on `StructInfo`, not on every expression node.

## Diagnostics

Unknown members: `CLPP0604`. Optional assigned to non-optional: `CLPP0201`.

## LSP

`get_type` / `get_members` / `type_of_expr` read the interned database.

## Codegen

Luau types are a lowering of the same `TypeId`s (`semantic::luau_type`).

## Tests

`tests/core.rs`: optional union, guard narrowing, using-alias `TypeId`, inheritance members.

## Documentation

`docs/types.md`, `docs/architecture/FEATURE_CHECKLIST.md`.
