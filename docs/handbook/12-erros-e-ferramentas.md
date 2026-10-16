---
title: Errors and tools
description: Read diagnostics, distinguish proposed identifiers, and use the tools available in the current compiler.
---

# Errors and tools

A useful diagnostic identifies the problem, points to its cause, and suggests a correction. The presentation below describes a design goal, rather than a guaranteed transcript of current compiler output.

> **Status:** the `CLPP...` and `CLUAU...` identifiers on this page are **proposed**, not verified compiler identities. `CLUAU...` entries belong to the example engine transpiler use case. The official compiler executes bytecode on the CL++ VM. `clpp fmt`, `clpp fix --def`, `clpp build`, and `clpp compile` are not available commands in the verified CLI; there is no official Luau emission path.

## Three goals

1. **Report the cause before its consequences.** Parser recovery and error types can reduce cascading messages, but cannot guarantee exactly one message per problem.
2. **Suggest a concrete action.** For example, suggest a typed declaration or `auto` when a declaration is outside the target syntax.
3. **Separate errors from style.** The current lexer recognizes `and`/`or`/`not`. Preferring `&&`/`||`/`!` and providing style hints are separate tool contracts.

## Severity levels

| Level | Meaning | Intended effect |
|---|---|---|
| Error | Invalid code or a violated contract | Prevent execution or publication of the invalid artifact |
| Warning | Possible issue or migration concern | Require review; project policy determines whether it blocks |
| Hint | Valid form with a preferred alternative | Guide editing without invalidating the program |

A developer implementing a transpiler must define which errors block its output. This table does not establish an emission policy in the official compiler.

## Read a diagnostic

Example of a **proposed** presentation:

```text
CLPP0201: optional<Dir> cannot be used as Dir without checking presence
   --> Maze.clp:42:17
  42 | Dir d = p.Queued;
     |         ^^^^^^^^ may be null
     = help: check presence before accessing the value
```

Read the identifier, message, filename, line, and column. Correct the reported cause and compile again. Narrowing through `guard` and its relationship to `optional<T>` also need compiler alignment.

## Proposed diagnostic catalogue

**No row asserts current support or a final identifier.** The catalogue preserves suggestions from the supplied material for future implementation work.

| Proposed identifier | Proposed level | Intended contract | Suggested correction |
|---|---|---|---|
| `CLPP0201` | Error | Optional value used as present without checking | Check presence before use |
| `CLPP0501` | Error | Selection omits required alternatives | Add cases or an allowed fallback |
| `CLUAU_AUTH001` | Error | Example transpiler call crosses server/client contexts | Review authority and host communication |
| `CLUAU_PAR001` | Error | Example transpiler finds an incompatible parallel effect | Separate calculation from state updates |
| `CLPP0901` | Warning | Old data declaration encountered during migration | Review manually for `def` |
| `CLPP0902` | Error | `let` used in target-language code | Declare with a type or `auto` |
| `CLPP0903` | Warning | Callback lacks required signature information | Define a function type with `def` |
| `CLPP0904` | Error | Definition inaccessible outside its module | Review visibility in the `.clp` source module |
| `CLPP0905` | Error | Incompatible signature | Review arguments and return type |
| `CLPP1001` | Hint | Logical alias used where style prefers symbols | Review for `&&`, `||`, or `!` |
| `CLPP1002` | Hint | Alternative `!def` spelling used | Prefer `def`; alias support remains pending |

A catalogue advertised as implemented must be generated or checked against actual compiler definitions. This design table is not a manifest of available features.

## Tolerant syntax

| Form | Preferred form | Status |
|---|---|---|
| `a and b` | `a && b` | Lexer recognizes the alias; style hint is separate |
| `a or b` | `a || b` | Lexer recognizes the alias; style hint is separate |
| `not a` | `!a` | Lexer recognizes the alias; style hint is separate |
| `!def` | `def` | Proposed alias requires implementation |
| Old data keyword | `def` | Manual migration; review behavior separately |
| `let` or `mut` | Type/`auto`, with `const` for fixed values | Rejected by the current parser |

## Current tools

```text
clpp program.clp
clpp-vm --repl
clpp-vm --lsp
clpp-vm --help
clpp-vm --version
```

`clpp program.clp` compiles **and executes** the program. Use `clpp-vm --check program.clp`
for analysis without execution. The language server powers editor diagnostics and
completion. The current syntax is implemented; examples explicitly marked as
future proposals or host contracts remain separate from supported language forms.

## Proposed formatting and migration tools

`clpp fmt file.clp` and `clpp fix --def` are proposed commands, **not executable instructions today**. A formatter should preserve behavior and be idempotent. A migration tool must review construction, types, and object semantics; replacing the old keyword with `def` does not guarantee equivalence.

Until these tools exist, follow the manual review in [Reference](14-referencia.md), consult [Implementation status](../reference/status.md), and validate supported features with the installed version.

## Debugging

- Read the first error and check its location.
- Use `post`, `warn`, and `report` to inspect execution; routing depends on the runtime or host.
- Run a supported program with `clpp program.clp`, or inspect editor diagnostics.
- When building a transpiler as a CL++ application, inspect **that application's output** to debug its translation. The official VM compiler does not emit a `.luau` file for you.

See [Transpilers and engine use cases](13-roblox-e-cluaupp.md) for that application boundary.
