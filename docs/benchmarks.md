---
title: Benchmarks
description: Fair compile + optimization methodology. CL++ high-level opts; Luau/native low-level. No dishonest baselines.
---

# Benchmarks

Architecture: [OPTIMIZATION.md](architecture/OPTIMIZATION.md) · [RFC 0011](https://github.com/KartzRbx/CLPP/blob/main/rfc/0011-optimization-pipeline.md).

## The rule

**CL++ proves and specializes → Luau optimizes bytecode → Roblox may emit native.**

Do **not** compare `buffer + @native` CL++ against naïve Luau tables and call it “8×.” Every published number needs a fair baseline column.

## Fair baselines (A–F)

| ID | Baseline |
| --- | --- |
| A | Idiomatic Luau |
| B | Hand-tuned Luau (expert) |
| C | roblox-ts → Luau (when available) |
| D | CL++ **opts off** (future flag) / today’s emit without claiming magic |
| E | CL++ **Phase 0+ opts** (fold / DCE / inline / scalar / loop / mono / layout hints) |
| F | E + Roblox **selective** `@native` via Cluaupp using `nativeHints` |

Baseline **D**: `clpp compile --no-opt` or `CLPP_NO_OPT=1` / JSON `"optimize": false`.

## Suites (targets, not marketing)

| Suite | What | Internal goal |
| --- | --- | --- |
| **Numeric** | millions of float / distance / damage ops | ≥2× idiomatic when opts+native apply |
| **Data-oriented** | homogeneous particles / SoA-ready | 1.5–3× + fewer allocations |
| **Realistic** | NPC-like update (no fake APIs) | measurable frame CPU drop |
| **Memory** | allocs, GC, code size, P50/P95/P99 | clear footprint story |

Studio timing uses MicroProfiler; prefer **consistency** (P95/P99), not only average FPS.

## What Phase 0 already proves (compiler unit)

`cargo test --offline --test opt_fold` — fold, DCE, inline, scalar, mono, `nativeHints`.

`cargo test --offline --test bench_compile` — end-to-end compile latency + type-check catches.

### Type-check differential (not runtime)

| Mistake | Luau alone | CL++ |
| --- | --- | --- |
| `optional<Player>` → `Player` | nil crash later | **CLPP0201** |
| private field outside struct | silent | **CLPP0401** |
| generic bound miss | no check | **CLPP0901** |
| `static_assert(false)` | N/A | **CLPP0701** |
| bare method without `@` | wrong self | **CLPP0101** |

Latest harness: **5/5** caught · results in [`benchmarks/results.json`](benchmarks/results.json) (also `/CLPP/benchmarks/results.json` on Pages).

### Compile throughput (debug, local)

| Workload | Median | ≈ LOC/s |
| --- | --- | --- |
| `examples/syntax/features.clp` | ~90 ms | ~1000 |
| synthetic 200 stmts | ~350 ms | ~1100 |
| Session identical re-check | ~0.03 ms | hash skip |

```bash
cargo test --offline --test opt_fold -- --nocapture
cargo test --offline --test bench_compile -- --nocapture
```

## Phase 0 opts in emit (examples)

```clpp
const float BASE_DAMAGE = 120;
const float CRIT_MULTIPLIER = 1.5;
float Damage() { return BASE_DAMAGE * CRIT_MULTIPLIER; }
```

→ Luau can contain `180` (folded), not a runtime multiply of two upvalues.

```clpp
const bool DEBUG = false;
void F() { if (DEBUG) { post("secret"); } post("ok"); }
```

→ `"secret"` branch removed.

## What we are not claiming

- Not “faster than Luau’s optimizer at Luau.”
- Not Studio frame times until Cluaupp runs suites A–F in-engine.
- Not buffer/SoA/`@native` wins until those passes exist and are measured fairly.

## Related

[Modules](modules) · [Type system](architecture/TYPE_SYSTEM) · [Optimization](architecture/OPTIMIZATION)
