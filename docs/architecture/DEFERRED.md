# Deferred after MVP

These 310-report items are **specified, not implemented** in this pass. Do not start them until binder + `TypeId` + checker + modules + LSP queries + optional platform are stable and `cargo test` stays green.

## HIR / IR (065–067, 211–217)

No full HIR / SSA between AST and Luau yet. The emitter still walks the CL++ AST. **AST-level high-level opts** (fold / DCE) are allowed per [RFC 0011](../../rfc/0011-optimization-pipeline.md) — that is not “invent LLVM.” A Luau AST + precedence printer can wait.

## Incremental queries (022–025 partial, 108–112, 231–233)

`Session` hashes a file and skips rebuild on the same bytes. That is not rustc queries, not incremental parse, not a disk cache. RFC 0006.

## Performance (092–095, 241–258)

No arena rewrite yet (`src/ice.rs` holds an ICE report stub + `ArenaHint`). No parallel checker. ICE dump protocol is the stub `IceReport` plus existing miette errors.

## Advanced language (291, 293–294, phase 14) → Intent System

Superseded as a grab-bag by **[RFC 0008](../../rfc/0008-intent-system-roadmap.md)** / [INTENT_SYSTEM.md](INTENT_SYSTEM.md).

Do **not** start effects, typestate, metaclasses, or regions until the MVP TypeId checker stays green and each piece has its own typed RFC. **Comptime + reflection** is opened as [RFC 0009](../../rfc/0009-comptime-reflection.md) (Intent Phase A #1). Phase C items (group borrowing, Verona concurrency, async drop) stay research-only.

## Explicitly not in CL++ (295)

C++ templates, macros, exceptions, pointers, async as a runtime, LLVM, GC, a custom VM, a new lexer, `=>` from the packaged `examples/basic.clpp` spec sample.

## Cluaupp product work (phase 12 host)

Watch/Rojo/project graphs that are not `src/platform` belong in the Cluaupp repo/tool, not in more compiler IR.
