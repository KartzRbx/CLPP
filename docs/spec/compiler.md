---
title: Compiler pipeline
description: How CL++ source becomes Luau. There is no bytecode VM.
---

The shipped Rust compiler uses the Pest parser, AST, binder, type checker and Luau emitter shown below. The rowan crates are an experimental replacement pipeline; see [Rowan CST](../compiler-architecture). The C++20 VM released as 0.10.x is a separate product.

How this maps to the [Luau](https://github.com/luau-lang/luau) repo: [Luau engineering map](../architecture/luau-mapping).

```mermaid
flowchart TD
  src["Source .clpp / .clh / .clp"] --> pp["Preprocessor directives and pragma"]
  pp --> pest["Pest grammar"]
  pest --> ast["AST plus spans"]
  ast --> binder["Binder: symbols and scopes"]
  binder --> types["Type database"]
  types --> checker["Type checker / narrowing"]
  checker --> analysis["Language service"]
  checker --> emit["Luau codegen"]
  emit --> out[".luau file"]
  analysis --> api["clpp api complete / hover / symbols"]
  api --> lsp["editors/vscode LSP"]
  out --> rojo["Rojo / Cluaupp"]
```

## Stages

| Stage | Where | Output |
| --- | --- | --- |
| Pragma | `src/preprocess` | One translation unit; comments keep source lines |
| Parse | `src/parser/grammar.pest` | AST (recovery for the IDE after the first Pest error) |
| Analysis | `src/analysis` + `src/session` + `src/checker` | Diagnostics, symbols, completions, hover, outline |
| Binder | `src/binder` + `src/symbols` + `src/types` | Symbol table and interned types |
| Builtins | `src/builtins` | One table for emit, manifest, and IDE |
| Emit | `src/codegen/emit` | Luau (`function Class:Method`, `self`, `game:GetService`) |
| Editor | `editors/vscode` | LSP calls `clpp api complete` |

## Host tools

Cluaupp (and anything else) should use:

```bash
clpp api compile      # JSON stdin/stdout
clpp api serve        # one JSON request/artifact per line
clpp api complete     # { source, fileName, line, column } 1-based
clpp api hover
clpp api symbols
clpp api definition
clpp manifest         # extensions, tags, operators, builtins
clpp fmt              # indent
clpp watch            # recompile on change
```

See [Cluaupp 0.8.0](../cluaupp-080), [Cluaupp 0.7.0](../cluaupp-070), [Update Cluaupp](../cluaupp-032), [CL++ + luau-lsp](../architecture/luau-lsp), and [Cluaupp support](../cluaupp-support). Current compiler: see [Version and contract](../version).
