---
title: Luau compiler API contract
---

The compiler and JSON protocol have independent SemVer versions. Run `clpp api manifest` to inspect `version` and `contractVersion`. Hosts support a contract major explicitly; additive fields require a minor increment and corrections require a patch. No contract change alters the original language syntax.

## Compile

`clpp api compile` accepts JSON on stdin and returns one CompileArtifact. `clpp api serve` accepts newline-delimited CompileRequest objects in one process; it emits one compact artifact per nonempty input line and flushes immediately. Invalid JSON produces CLPP0005 and the server continues. EOF shuts it down. Responses retain input order.

```json
{"source":"link @clpp.libs.roster as Roster; void init() { auto roster = new Roster(); }","fileName":"main.server.clpp","strict":true,"optimize":false,"libRoot":"ReplicatedStorage.CluauppLibs"}
```

`libRoot` is a dotted identifier path, not a Luau expression. It defaults to `ReplicatedStorage.CluauppLibs`. The emitter binds ReplicatedStorage with GetService and emits `require(ReplicatedStorage.CluauppLibs.Roster)`. Hosts do not rewrite library paths, constructors or indentation.

## Diagnostics

Every compilation failure has structured diagnostics: `code`, `span`, `severity`, `message` and `help` (suggested correction). Legacy `line` and `column` remain. Coordinates count Unicode characters, are 1-based, and range ends are exclusive. Use codes instead of parsing messages. `clpp explain CLPP1103` explains why propagation requires a Result return type.

## Source map

`sourceMap` is recorded during printing, with `luauLine`, `luauColumn`, `clppLine`, `clppColumn`, `file`, and originating `span`. Locations anchor generated declarations and statement expansions; they are not a token-by-token debugger map. Generated runtime helpers have no invented source location. Columns count a tab as one character. Any host transformation of the output must update mappings itself.

## Language invariants

The original module form is `link`. Removed `import {…} from` and `#include` receive a suggestion to use link. The binder resolves declarations before bodies. Linked source interfaces are read before checking consumers; missing modules and cycles produce CLPP0801 and CLPP1001.

Option uses the existing `optional<T>` / `Option<T>` forms, Some and None. Result uses a tagged Luau union, preserving an Ok value of nil or false. The existing postfix `?` returns Err from the enclosing Result function. Match must cover Some/None, Ok/Err, or enum variants, or have a wildcard. RFCs 0014–0018 remain deferred.

The machine-readable TypeScript definitions live in `support/cluaupp.d.ts`; golden tests compare emitted source exactly, run the official luau-analyze in strict mode, and execute behavioral assertions in Luau.
