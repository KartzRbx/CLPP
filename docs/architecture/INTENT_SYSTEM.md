# Intent System — CL++ language identity

> How much of a program’s architecture and invariants can we prove at compile time **without** sacrificing ergonomics?

CL++ does **not** win by inventing alien syntax or by copying Rust. It wins by **synthesizing** research that already showed value (C++/Rust comptime+reflection, Herb Sutter metaclasses, Koka effects, Swift regions/move-only, Carbon checked generics, Verona concurrent ownership, contracts, typestate) into one coherent layer:

```text
                    Intent / Semantics
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
        Type System   Effect System   Contracts
            │              │              │
            └──────────────┼──────────────┘
                           ▼
              Semantic Interfaces (signature idea)
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
         CL++ (platform-agnostic)    Cluaupp (interprets intents)
```

CL++ stays **Roblox-free** in the core. Cluaupp maps intents → Server / Network / DataStore / DataModel permissions — see [INTENT_SYSTEM.md](INTENT_SYSTEM.md).

## Research map (inspiration, not copy)

| Idea | Origin | Role in CL++ |
| --- | --- | --- |
| Comptime + reflection | C++ / Rust 2026 direction | Generate serializers, validators, stubs **without** runtime reflection |
| Metaclasses | Herb Sutter / C++ | `class(service)`, `class(remote)` as **transforms**, not hard-coded AST kinds |
| Effects in types | Koka | Pure vs `DataStore` / `Network` / `Mutation` as part of the signature |
| Region isolation | Swift 6 | Match / arena / transfer regions |
| Concurrent ownership | MS Verona | `isolated` + safe transfer |
| Checked generics | Carbon | Verify generic body against bounds at **definition** |
| Non-copyable / move | Swift / Rust | Connections, janitors, handles |
| Contracts | MSR / C++ | `requires` / `ensures` (prove / assert / debug) |
| Typestate | classic | `Trade<Open>` → `Trade<Locked>` as types |
| Group borrowing | Vale / Carbon research | **Experimental only** |
| Async drop | Rust research | Later — resource scopes |
| Incremental queries | Salsa / rust-analyzer | Tooling (RFC 0006), not language surface |

## Semantic Interfaces (the synthesis)

Today:

```text
interface Storage { save(...); load(...); }
```

Intent-level:

```text
interface TradeProcessor
  capability Server
  effects Mutation, DataStore
  consumes Trade<Processing>
  produces Trade<Completed>

  process(trade: Trade<Processing>) -> Trade<Completed>
```

The compiler checks the contract. Cluaupp **interprets** it for Roblox. That is the product identity — not “more keywords.”

## Example intent (target shape, not MVP syntax)

```text
fn ConfirmTrade(trade: Trade<Ready>) -> Trade<Completed>
  requires trade.owner == caller
  effects [Mutation, DataStore]
  consumes trade
  ensures result.state == Completed
```

## Hard rules

1. **Spec before features** — formalize core semantics; do not bolt on 15 research ideas at once.
2. **FEATURE_CHECKLIST** — grammar alone ≠ implemented (binder + `TypeId` + checker + emit + tests).
3. **No alien runtime** — Luau backend; prefer zero-cost / erase abstractions (see DONT_REINVENT / DEFERRED).
4. **Cluaupp interprets; CL++ does not embed Roblox** in Intent core.
5. **Finish current MVP checker** (`pass` → `typed`) before shipping further Intent pieces; Phase A #1 comptime is [RFC 0009](../../rfc/0009-comptime-reflection.md).

## Phased roadmap

See [RFC 0008](../../rfc/0008-intent-system-roadmap.md). Mini-RFCs: [0009 comptime](../../rfc/0009-comptime-reflection.md), [0010 checked generics](../../rfc/0010-checked-generics.md), [0012 Option/Result](../../rfc/0012-option-result.md).
