# Don't Reinvent the Wheel — CL++ audit

Rule for the two products and the ecosystem:

| Layer | Owns |
| --- | --- |
| **CL++** | Only what defines the language |
| **Cluaupp** | Only what connects CL++ to Roblox |
| **Ecosystem** | Mature standards / libraries / tools |

## Own (language IP)

| Part | Status |
| --- | --- |
| CL++ syntax / Pest grammar | Own — Pest is the engine; grammar is ours |
| Semantics, type rules, OOP | Own — `binder` / `types` / `checker` |
| Module system | Own — `modules` + `Session` |
| AST + semantic model | Own — `analysis` / `CompilerApi` |
| CL++ → Luau lowering | Own — `codegen` / `emitter` (not LLVM) |
| IDE queries (complete/hover/…) | Own — semantic engine; LSP is only the adapter |
| Module imports | Own — `import { Name as Alias } from "path"` (RFC 0003) |

## Reuse (already or target)

| Part | Choice |
| --- | --- |
| Parser engine | Pest (done) |
| JSON | serde / serde_json (done) |
| CLI | clap (done) |
| Errors | miette (done) |
| **LSP protocol / JSON-RPC framing** | **`tower-lsp-server`** (stdio adapter; no hand-rolled `Content-Length`) |
| Roblox API / DataModel / Rojo / API dump | Cluaupp + platform prelude — not CL++ core |
| Roblox IntelliSense on emitted Luau | luau-lsp + Rojo sourcemap (complementary) |
| Editor incremental parse (future) | Tree-sitter grammar for editors; Pest stays compiler parser |

## Explicitly out of scope

- Hand-rolled JSON-RPC / LSP wire format
- LLVM “for professionalism” while the backend is Luau
- C++ templates / macros / pointers / async runtime / custom VM / new lexer
- HIR / query engine / arena rewrite — see [DEFERRED.md](DEFERRED.md)

## Architecture

```text
CL++ Compiler
      │
Semantic Model (analysis / Session / TypeId)
      │
 ┌────┴────┐
 ▼         ▼
compile   LSP adapter (tower-lsp-server)
              │
         VS Code / Cursor
              │
           Cluaupp → Roblox ecosystem
```

## First fix (done)

`src/lsp/server.rs` was a manual Content-Length + method switch. It now implements `tower_lsp_server::LanguageServer` and keeps CL++ logic in `analysis` / `CompilerApi`.

## Examples

`examples/` is **language-only** (syntax + modules). Roblox / Studio samples belong in the Cluaupp repo.
