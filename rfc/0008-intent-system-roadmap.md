# RFC 0008 — Intent System roadmap

Status: Accepted (design direction) — **not** an implementation mandate for the current MVP sprint

## Problem

Random feature accretion risks an unspecifiable language. Research across Rust, Swift, C++, Carbon, Verona, Koka, Vale shows valuable directions; CL++ should adopt a **synthesis** (Intent System) with a phased plan, not a kitchen sink.

## North star

Prove architecture and invariants at compile time without killing ergonomics. Platform-agnostic intents; Cluaupp maps them to Roblox.

## Relationship to MVP

**Do first (current engineering track):** finish TypeId checker migration, solid LSP queries, source maps, modules (`import { } from`). See pass → typed work and RFC 0001–0007.

**Then** open Intent Phase A RFCs with formal semantics.

## Phase A — core Intent (after MVP checker solid)

| Piece | Inspiration | CL++ role | RFC |
| --- | --- | --- | --- |
| Comptime + reflection | C++ / Rust | Inspect structs; generate serializers / validators / docs **at compile time** | [0009](0009-comptime-reflection.md) |
| Checked generics | Carbon | Generic bodies checked against bounds at definition | [0010](0010-checked-generics.md) |
| `Option` / `Result` as core | modern langs | Explicit error/optional surface | [0012](0012-option-result.md) |
| Pattern matching (richer) | — | Extend beyond current `match` / `switch` | — |
| Typestate | classic | `Trade<Open>` style state-carrying types | — |
| Contracts | MSR / C++ | `requires` / `ensures` (prove / runtime / debug tiers) | — |
| Effects (lite) | Koka | Effect sets on functions; inference via call graph | — |
| Nominal / distinct types | — | Stronger newtypes | — |
| Non-copyable / move | Swift / Rust | Unique resources (connection, transaction, handle) | — |

Each item needs its **own** mini-RFC (syntax + typing rules + emit strategy + tests) before landing.

## Phase B — when Phase A semantics are stable

| Piece | Inspiration |
| --- | --- |
| Region-based isolation | Swift |
| Capabilities (full) | effects × authority |
| Compile-time metaclasses | Herb Sutter (`class(service)`, `class(remote)` as transforms) |
| Resource scopes | RAII / using |
| Incremental compiler queries | Salsa (RFC 0006 deepening) |

Metaclasses are how Cluaupp-facing patterns (`service`, `remote`) stay **libraries/transforms**, not forever hard-coded in `src/ast`.

## Phase C — research / experimental

| Piece | Notes |
| --- | --- |
| Concurrent ownership | Verona |
| Group borrowing | Do **not** implement now |
| Generational references | Research |
| Async drop | After resource model exists |
| Advanced effect handlers | Full Koka-style control effects |

## Explicitly out (unchanged)

C++ templates, macros, exceptions, raw pointers, async-as-runtime VM, LLVM backend, custom GC/VM, new lexer — [DEFERRED.md](../docs/architecture/DEFERRED.md).

## Cluaupp bridge

```text
CL++ Intent  →  Type / Effect / Contract checks  →  Cluaupp interprets
  Mutation → server authority
  DataStore → server only
  Network → remote contract
  InstanceMutation → DataModel permission
```

Language RFCs for `remote` / context attrs remain thin hooks; product behavior stays in Cluaupp ([RFC 0007](0007-cluaupp.md), [CLUAUPP_ARCHITECTURE.md](../docs/architecture/CLUAUPP_ARCHITECTURE.md)).

## Success criteria for “Intent is real”

1. Written typing rules for effects + contracts + typestate (even if subset).
2. At least one Semantic Interface example compiling with checks (no Roblox required).
3. Cluaupp can consume emitted metadata (effects / capabilities) without parsing CL++ ad hoc.
4. `cargo test` green; no silent dual checker regressions.

## Documentation

[INTENT_SYSTEM.md](../docs/architecture/INTENT_SYSTEM.md), [LANGUAGE_DESIGN.md](../docs/architecture/LANGUAGE_DESIGN.md), [DEFERRED.md](../docs/architecture/DEFERRED.md).
