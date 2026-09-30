# Phase 1 — Roblox-free front end

Phase 1 is a hand-written lexer and parser that sit beside the existing Pest pipeline. The shipping `clpp` binary is unchanged. The new code lives in `clpp_front` and does not name a host runtime.

## Pipeline

Each stage is a pure function of its inputs:

| Function | Input | Output |
| --- | --- | --- |
| `lex` | source text | tokens and lex diagnostics |
| `parse_tokens` | source text and tokens | lossless CST and parse diagnostics |
| `lower` | CST | typed AST |
| `parse` | file name and source | all three, diagnostics merged |

Nothing in these functions reads a database or a host API. A later incremental layer (salsa 0.28) can memoize them without changing the signatures.

## Syntax tree

The parser is recursive descent. Expressions use Pratt parsing from `??` through `**`. Assignment, the postfix try operator `?`, colon-calls, and `?:` stay outside Pratt so they match the existing grammar, including `cond ? a : b` and `(a + b)?`.

The CST is a rowan green/red tree (0.17). Every byte of the file is one token, including whitespace and comments, and every token is placed in the tree. `syntax.text()` equals the source, including on garbage. A typed AST is lowered on top: types are `Ty` nodes, not strings.

## Crate layout

The suggested name `clpp-syntax` collides with the existing stub package `clpp_syntax`. Cargo treats hyphen and underscore as the same package name, and that stub still special-cases host links and is what `clpp_cli` calls. Phase 1 is therefore the package `clpp_front` (binary `clpp-front`). It depends on rowan 0.17 directly. The workspace pin stays on rowan 0.15 so the stubs are left alone.

Later crates, not built here:

- `clpp-core` — a neutral prelude, the way TypeScript's `lib` is separate from `--noLib`
- `clpp-target-luau` — a stdlib profile of `math` / `string` / `table` / `buffer` / `vector`
- `@clpp/roblox` — a package outside the compiler, generated from an API dump

A test walks `crates/clpp_front/src` and fails if a host-runtime name appears (`Instance`, `GetService`, `game`, `workspace`, `RBXScriptSignal`, `Vector3`, `CFrame`, `Janitor`, `task.`, `Rojo`, and the same family).

## `//=` and comments

`//` starts a line comment unless the third character is `=`, in which case the token is `//=` (floor-division assignment). `///` and `//!` stay comments. `//==` is `//=` followed by `=`.

The Pest grammar lists both a `//` comment and the operator `//=`, but comments are tried first, so `//=` never reached the parser. Dropping the operator, or changing the comment syntax, would break one of those two spellings. The lookahead keeps both.

`---` is maximal munch. The lexer takes `--` (decrement) and then `-`, so `---x` is not three unary minuses. `- - -x` with spaces is three unaries. That matches the token `MinusMinus` and is not rewritten into three minuses.

## What is not a keyword

`signal`, `observable`, `spawn`, `parallel`, `delay`, and `defer` are ordinary names. `signal<int>` is a generic type a target library can define. `observable int x` is a parse error, not a declaration form.

`~>` is not an operator. `~` is an unknown character and a lex error. Encoding it as an annotation would keep a host operator in the grammar; leaving it out matches the rule that the core has no host syntax.

`[[name]]` and `[[name("string")]]` are generic attributes. They carry no target meaning, so `[[server]]` and `[[client]]` are not keywords. `@name` is a generic sigil (`@field`, `@coins`), not a host link.

## Modules

`link` is the only module form. `import` and `#include` are recognized only so they can be rejected with `CLPP0801` and a note to write `link "./file.clh" as Name;`. That is a language decision, not an open question. Other `#` lines (`#pragma once`, `#if`) are uninterpreted `Directive` nodes. The new front end does not run the preprocessor.

The local binding name is one function:

- when `as` is present, the alias, and only the alias. `as 1` and `as;` diagnose and bind nothing; they do not fall back to the stem
- otherwise, for a path, the file stem with a trailing `.clpp`, `.clp`, or `.clh` removed (`./shared/Wallet.clp` binds `Wallet`, not `clp`), and only when that stem is an identifier
- otherwise, for `@a.b.c`, the last segment, when it is an identifier

`a.b.clp`, `.clh`, and `Main.server.clpp` are not identifiers. The link stays in the tree, `binding` is absent, and the diagnostic says to write `as Name`. Two links that bind the same name in one file are diagnosed. A package link's display form is `@a.b.c` and never a filesystem path.

Expressions, blocks, and types share a depth cap of 32. One more nest produces a single "nesting is too deep" diagnostic, and the rest of that construct is skipped in a loop so the stack does not grow with the input. That keeps a few thousand parentheses inside a 256 KiB stack. `=`, `**`, and `?:` chains are loops, so a few thousand of them do not recurse, do not hit the cap, and do not rescan the tail. A chain at one precedence is a single node (`a + b - c`, `a ** b ** c`, `a = b = c`) rather than a spine, which is also what keeps freeing the tree off the call stack. `+` and `-` share that node and associate left to right. `**` and assignment associate right to left. A tighter operator is still a nested node, so `a + b * c` is `+` whose right operand is `*`.

The shipping `clpp emit` uses that same binding rule. `link "./shared/Wallet.clp" as Purse` emits `const Purse = require(...)`, and a stem that is not an identifier is not a `const` name. An `@package` link is `CLPP0802` and is never rewritten into a `script.Parent` path. The docs already distinguish `@clpp` (a prelude comment) from `@game` (`GetService` plus `require`); this emitter does not guess a Rojo layout, so both stay a diagnostic.

## Diagnostics

A diagnostic has a byte span, a severity, an optional code, and an optional help note. A parse collects every error; it does not stop at the first one.

The short renderer is `file:line:col` with the source line and carets. `LineIndex::line_col` and `line_col_in` both count columns as 1-based Unicode scalar values (`é` is one column, not two bytes). `api_view` uses that same count. Mapping those columns to LSP UTF-16 code units is a later concern; this crate does not do it. The `clpp-front` CLI also prints a codespan-reporting report (primary label, source, help), in the shape described by the rustc diagnostics guide.

`ApiDiagnostic` is the field set a future `clpp api compile` object can copy: `message`, `line`, `column`, `end_line`, `end_column`, `severity`, `code`, `help`. Today's `CompileDiagnostic` is `{message, line, column, severity, code?, help?}` and `SourceMapLine` is `{luauLine, clppLine, file}`. The extra end positions are there so a span-based source map can grow into the same JSON without a second diagnostic type.

## Tooling around the front end

`clpp-front` reads a file or stdin and prints tokens, the CST, or the lowered AST. Exit status is 1 when any error diagnostic was produced.

`cargo test` no longer writes `docs/benchmarks/results.json`. That file is written only when `CLPP_WRITE_BENCH` is set.

CI runs the Node intellisense test, `cargo test --workspace`, and the libs check as separate jobs, so a Node failure does not skip the Rust suite. Node is 22. Minimum supported Rust is 1.88, recorded as `rust-version`. CI stays on stable.

## Roadmap

Not implemented in this phase:

- a single strict checker with `TypeId`, `int` and `float` distinct
- HIR and a bidirectional checker
- a small typed IR, then a Luau AST and printer, with Luau performance rules
- pure-Luau codegen
- `.d.clpp` extern declarations, the way `.d.ts` describes a host
- a `Target` trait, and `@clpp/roblox` generated from the Roblox API dump
- golden tests that run the generated Luau with the `luau` binary
- salsa on the pure stage functions above

## Open questions

- `#if` / `#pragma` are stored as directives and are not executed. The preprocessor stays on the old pipeline until a later phase owns it.
