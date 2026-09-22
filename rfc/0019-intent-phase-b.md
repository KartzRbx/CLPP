# RFC 0019 — Intent Phase B (regions / capabilities / metaclasses / scopes)

Status: Deferred until Phase A RFCs 0013–0018 are stable

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Scope

- Region isolation (no shared mutable across regions)
- Capabilities (full, beyond host tags in `src/platform`)
- Metaclasses: `class(service)`, `remote` as transforms
- Resource scopes (RAII-like `using` / scope guards)
- Salsa-depth incremental (see also RFC 0006)

## Non-goals

Verona-style group borrowing / Phase C research until A is done.
