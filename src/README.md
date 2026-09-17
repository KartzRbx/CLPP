# src/

CL++ compiler in Rust. Pest covers the lexicon; there is no separate lexer in this phase.

| Path | Role |
| --- | --- |
| `main.rs` | clap CLI (`compile` / `build` / `api` / `install`) |
| `parser/grammar.pest` | PEG grammar |
| `parser/mod.rs` | pest pairs → AST |
| `ast/` | language enums |
| `preprocess.rs` | `#include` / `#pragma` |
| `semantic/` | Roblox types, `:` vs `.` |
| `codegen/luau.rs` | AST → Luau |
| `compile.rs` | file → Luau pipeline |
| `error.rs` | miette errors |
