---
title: Compiler architecture
description: Cargo workspace, rowan lossless CST, and an LSP that keeps working on a broken line.
---

# Compiler architecture

CL++ is a compiler, not a text preprocessor. The new pipeline is a Cargo workspace. Each crate has one job. The editor reads the same tree the checker reads.

| Crate | Role |
| --- | --- |
| `clpp_syntax` | Token kinds and the rowan green tree |
| `clpp_parser` | Hand-written parser, Pratt expressions, error recovery |
| `clpp_hir` | Desugared items and dead-name filtering |
| `clpp_ty` | Types, `RunContext`, `CLUAU_AUTH`, `CLUAU_PAR` |
| `clpp_codegen` | Luau, only after the checker is clean |
| `clpp_lsp` | Completion and diagnostics from the CST |
| `clpp_cli` | `build`, `lsp`, `lint`, `fmt`, `doc` |

## Lossless CST

[rowan](https://docs.rs/rowan) stores a red/green tree. Whitespace and comments stay in the tree. A syntax error becomes an `Error` node. The parser then synchronizes on `;`, `}`, or `)` and keeps parsing. The prefix you already typed is not thrown away.

```clpp
let x =
```

That line is incomplete. The tree still contains `let`, the name, and `=`, plus an `Error` node where the expression should be. Completion does not stop because that node is an `Error`.

## LSP on a broken line

`Janitor janitor;` followed by `janitor.` still asks the symbol index for `Add`, `Cleanup`, and `Destroy`, even when the declaration line itself is an `Error` node. The index is filled from `link` targets: `@clpp` in the standard library, `@game` from `default.project.json` or `clpp.toml`, and `./` paths next to the file.

## What is not a keyword

Methods are `Type::Method`, not a Rust `impl` block. `option` in the highlighter is the same family as `optional` / `Option<T>`. `@layout` is reserved next to `@client`, `@server`, and `@parallel`.
