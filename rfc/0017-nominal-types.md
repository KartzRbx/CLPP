# RFC 0017 — Nominal / distinct types

Status: Draft — surface reserved

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface (planned)

```clpp
newtype UserId = int;
newtype OrderId = int;
```

`UserId` and `OrderId` are not assignable without explicit cast, despite identical representation.

## Emit

Erase to underlying Luau type; keep type aliases in `--emit-types` / native annotations when useful.
