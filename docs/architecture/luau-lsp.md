---
title: CL++ + luau-lsp
description: CL++ owns .clpp IntelliSense. luau-lsp owns emitted Luau plus a Rojo sourcemap.
---

CL++ 0.4 does **not** replace [luau-lsp](https://github.com/JohnnyMorganz/luau-lsp). The two servers sit on different files.

| You are editing | Language server | What it knows |
| --- | --- | --- |
| `.clpp` / `.clp` / `.clh` | CL++ pack (`clpp install`) | CL++ AST, scopes, `clpp api complete` |
| Emitted `.luau` | [luau-lsp](https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp) | Luau types, Roblox API, Rojo sourcemap |

IntelliSense **while you type CL++** comes from `clpp api complete` on the CL++ AST. luau-lsp cannot parse `.clpp`. It validates the **output** after compile.

## Editor setup (Roblox)

1. Install CL++ (`clpp-setup.exe` or `clpp setup`) and reload the window.
2. Install **Luau Language Server** (JohnnyMorganz.luau-lsp).
3. Point luau-lsp at a Rojo sourcemap if you use Rojo, for example `sourcemap.json` from `rojo sourcemap`.
4. Keep compiling with Cluaupp / `clpp api compile` so the `.luau` files luau-lsp sees stay in sync.

CL++ LSP trigger characters include `.`, `:`, `>`, `@`. Completions for locals, params, `Class::Method`, and `@this` fields come from analysis — not from regex on the current line.

## Optional CI check of emitted Luau

`clpp` does not bundle `luau-analyze`. If that binary is on `PATH`:

```bash
clpp compile path/to/file.server.clpp -o /tmp/file.server.luau
luau-analyze /tmp/file.server.luau
```

Or compile a tree and analyze the output directory:

```bash
clpp build src -o out
luau-analyze out
```

Treat failures as Luau-side issues (types, unknown globals in the **emit**). Syntax that failed in CL++ never reaches this step.

## What not to do

- Do not send `.clpp` buffers to luau-lsp.
- Do not expect the CL++ language server to load Roblox class dumps — that remains Cluaupp headers plus the editor catalog.
- Do not look for a CL++ bytecode VM. Performance of the language tool is parser + analysis + emit.

See the [Luau engineering map](luau-mapping) and the [compiler pipeline](../spec/compiler).
