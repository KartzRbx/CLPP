# src/

CL++ compiler in Rust. Pest covers the lexicon; there is no separate lexer in this phase.

CL++ is the language and compiler (Luau backend). Roblox lives in `platform` / `roblox` and is loaded by Cluaupp via `Session::with_roblox_platform()`. `Session::language()` is the core without that prelude.

| Path | Role |
| --- | --- |
| `main.rs` | clap CLI (`compile` / `build` / `api` / `install` / `lsp`) |
| `parser/grammar.pest` | PEG grammar |
| `parser/mod.rs` | pest pairs → AST |
| `ast/` | language enums (no Roblox type database; `GetService<T>()` is a Call) |
| `binder/` | AST → symbols and nested scopes |
| `symbols/` | `SymbolId`, scopes |
| `types/` | interned `TypeId` / `TypeKind` / assignability |
| `checker/` | resolve, narrowing, name-table pass, TypeId validation |
| `names.rs` | globals / Instance name tables / `luau_type` mapping |
| `session/` | project session, file cache, compiler queries |
| `driver/` | facade over `compile` + `session` |
| `emitter/` | facade over Luau codegen |
| `diagnostics/` | facade over diagnostic codes |
| `platform/` | optional host API (Roblox prelude) |
| `roblox/` | Roblox Instance metadata loaded by platform |
| `modules/` | quoted `#include` and `import { Name } from "path"` |
| `preprocess.rs` | `#include` / `#pragma` |
| `codegen/` | AST → Luau |
| `compile.rs` | file → Luau pipeline |
| `analysis/` | IDE facade over the type session |
| `lsp/` | language server; queries `CompilerApi`, does not emit |
| `error.rs` | miette errors |

Semantic API (`Session` / `CompilerApi`): `get_symbol`, `get_type`, `get_members`, `get_definition`, `get_diagnostics`.
