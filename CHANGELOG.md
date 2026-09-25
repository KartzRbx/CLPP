# Changelog

## 0.8.1

- Editor grammar highlights `import { Name } from "…"`, including `from` / `as` and the module path. Completions list `import`, `from`, and `as`.

## 0.8.0 — Intent + opt Phase 1 lite

Detalhamento completo: [`docs/CHANGELOG-0.8-DETAILED.md`](docs/CHANGELOG-0.8-DETAILED.md).

### Compiler
- Unified TypeId checker path (`check_unified`) with Result/Option assign rules (`CLPP0202`).
- RFC 0012/0013: `Ok`/`Err`/`Some`/`None`, `?` (`Expr::Try`, `CLPP1101`), exhaustive Option/Result `match` (`CLPP1102`).
- **Modules:** `link @clpp…`, `link @game…`, and `link "./path" as Name`. `import` and `#include` are rejected.
- Opt Phase 1 lite: SoA rewrite, buffer specialize hints, mono type-arg inference, inline cost heuristic, broader escape analysis, loop invariant hoist.
- Session: dependency graph + invalidate (RFC 0006 lite).
- Platform: `capabilities_from_hints` for Cluaupp.
- Emitter: preserve Option/Result-oriented Luau annotations for native.
- Grammar P0: structural try/ternary/colon precedence; remove `get_service` PEG; fix `function_type` params.

### LSP / IDE
- Rename, references, formatting, signature help wired.
- Completion/hover for Option/Result and Intent reserved keywords.

### Intent / docs
- RFCs 0013–0019 (pattern match shipping subset; contracts/effects/typestate/newtype/move/Phase B drafts).
- `docs/option-result.md`, `docs/cluaupp-host.md`, `support/cluaupp-bridge.ts`.
- ICE stub module; FEATURE_CHECKLIST matrix.

### Benchmarks
- Fair A–F fixtures remain compile-only; Studio P50/P95/P99 results marked pending honest measurement in `docs/benchmarks/results.json`.

## 0.7.0

Prior release: MVP modules, generics/comptime subset, opt Phase 0.
