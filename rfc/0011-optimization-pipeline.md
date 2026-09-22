# RFC 0011 — High-level optimization pipeline

Status: Accepted — Phase 0 (passes 1–8) shipping

Parent themes: Intent System ([0008](0008-intent-system-roadmap.md)), Cluaupp ([0007](0007-cluaupp.md))

## Problem

Trying to “beat Luau’s optimizer at Luau” is the wrong fight. Luau already has constant folding, upvalue opts, builtins, and peephole passes; Roblox adds selective **native codegen**. CL++ wins by **proving more before runtime** and emitting *simpler, type-stable, numeric-friendly* Luau — then letting Luau + native do low-level work.

## Pipeline

```text
CL++ Source
    → Typed AST (TypeId checker)
    → CL++ high-level opts (this RFC)
    → Luau lowering (emitter)
    → Luau compiler / --!optimize / @native (Roblox)
```

**Rule:** never throw away type information the backend could use. Prefer predictable locals and concrete arithmetic over dynamic tables when the program already proved it.

## Explicitly not doing

- A second Luau JIT / LLVM backend in `clpp`
- Blind `--!native` on every function (compile cost / memory / native code limits)
- Marketing “Nx faster than roblox-ts” without fair baselines

## Phase 0 — ship now (AST transforms)

| # | Pass | Status |
| --- | --- | --- |
| 1 | Constant folding | **shipped** (`src/opt/fold.rs`) |
| 2 | Constant propagation (const locals) | **shipped** |
| 3 | Dead branch / `if (false)` elimination | **shipped** |
| 4 | Selective inlining | **shipped** (`src/opt/inline.rs`) |
| 5 | Temporary / scalar replacement | **shipped** (`src/opt/scalar.rs`) |
| 6 | Loop invariant const hoist | **shipped** (`src/opt/loop_opt.rs`) |
| 7 | Generic monomorphization (explicit type args) | **shipped** (`src/opt/mono.rs`) |
| 8 | `@native` selection **hints** (artifact metadata) | **shipped** (`src/opt/native.rs` → `nativeHints`) |

Phase 0 mutates the **typed AST** before emit. A separate Opt IR can wait until these passes need SSA-like form ([DEFERRED.md](../docs/architecture/DEFERRED.md) updated: full HIR still deferred; AST opts allowed).

**Native policy:** hints only. Cluaupp decides whether to emit `@native` — never blanket `--!native`.

## Phase 1 — layout & native (partial in `clpp`)

| Piece | Status |
| --- | --- |
| Escape-aware Vector/InitList scalar | **shipped** (`escape` + `scalar`) |
| Layout / buffer / SoA **hints** | **shipped** (`layoutHints` on artifact) |
| Selective `@native` emit | Cluaupp consumes `nativeHints` |
| PGO | Cluaupp `profile` (later) |
| SoA / buffer codegen | later (measure first) |

## Fair benchmarks (required)

Suites (internal targets, not marketing claims):

| Suite | Goal |
| --- | --- |
| **A Numeric** | hot float/Vector math |
| **B Data-oriented** | many homogeneous particles |
| **C Realistic** | NPC-like update without fake APIs |
| **Memory** | allocations / code size / GC pressure (Studio) |

Baselines for every claim:

1. Idiomatic Luau  
2. Hand-tuned Luau  
3. roblox-ts → Luau (when available)  
4. CL++ baseline (opts off)  
5. CL++ optimized  
6. CL++ optimized + Roblox native (separate column)

Never compare buffer+`@native` CL++ against naïve tables without saying so.

Internal targets (aspirational): numeric ≥2× idiomatic; data 1.5–3×; clear allocation drops — **verify**, don’t invent.

## Implementation map

| Layer | Location |
| --- | --- |
| Docs | [OPTIMIZATION.md](../docs/architecture/OPTIMIZATION.md), [benchmarks](../docs/benchmarks.md) |
| Passes | `src/opt/` |
| Hook | `compile.rs` before `emit` |
| Tests | `tests/opt_fold.rs`, `tests/bench_compile.rs` |

## Success

- `const` arithmetic folds in emitted Luau
- `if (false)` / const-false guards drop dead bodies
- small pure calls inline; non-escaping InitLists become scalars
- explicit `Foo<T>` calls can emit `Foo__T`; artifact carries `nativeHints`
- `cargo test` green; docs state fairness rules
