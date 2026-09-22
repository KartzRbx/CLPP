---
title: Optimization architecture
description: CL++ does high-level opts; Luau and Roblox native do the rest. No second JIT.
---

# Optimization architecture

> **CL++ proves and specializes. Luau optimizes bytecode. Roblox may emit native — selectively.**

```text
        Intent / Types / Effects
                  │
                  ▼
           Typed AST
                  │
       ┌──────────┼──────────┐
       ▼          ▼          ▼
    Fold/DCE   (later)    (Cluaupp)
    prop/inln  layout     @native pick
       │          │          │
       └──────────┼──────────┘
                  ▼
              Luau emit
                  │
                  ▼
         Luau optimizer
                  │
                  ▼
        Roblox native (optional)
```

Full design: [RFC 0011](https://github.com/KartzRbx/CLPP/blob/main/rfc/0011-optimization-pipeline.md).

## Phase 0 (in `clpp` today)

1. **Constant propagation** of `const` / constexpr bindings  
2. **Arithmetic / bool folding** on proven constants  
3. **Dead branch elimination** (`if (false)`, const-false conditions)  
4. **Selective inlining** of small pure free functions  
5. **Scalar replacement** of non-escaping `InitList` / Vector temps  
6. **Loop-invariant const hoist** (before `while` / `for`)  
7. **Monomorphization** when call sites pass explicit type args  
8. **`nativeHints`** on `CompileArtifact` for Cluaupp (no auto-`@native`)  
9. **`layoutHints`** (DenseNumeric / BufferCandidate / SoACandidate)  
10. **`--no-opt` / `optimize: false`** for fair baseline D

These run on the AST after typecheck, before Luau emit (unless `--no-opt`).

## Not fighting Luau

Do not re-implement Luau peephole / upvalue opts. Preserve types, remove dead work, specialize when cheap. Selective `@native` is a **Cluaupp** decision using hotness + numeric-heaviness — not `--!native` on the whole game.

## Benchmarks

See [Benchmarks](../benchmarks) for fair A–F baselines and suites Numeric / Data / Realistic / Memory.
