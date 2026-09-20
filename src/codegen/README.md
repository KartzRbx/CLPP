# src/codegen/

Luau code generation from the AST.

| Path | Role |
| --- | --- |
| `emit/engine.rs` | `Emitter` — program, stmt, expr, header |
| `emit/helpers.rs` | builtin name map (`crate::builtins`) |
| `luau.rs` | re-export `emit` for existing `codegen::luau::emit` |

There is no CL++ IR or bytecode. Analysis lives in `src/analysis/`.
