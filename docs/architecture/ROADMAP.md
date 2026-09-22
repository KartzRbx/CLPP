# ROADMAP.md

## Fase 0 — Specification
Grammar, type semantics, module semantics, Roblox semantics, `@`, `~>`, `.:`.

## Fase 1 — Lexer
Tokens, keywords, operators, comments, documentation comments, spans, error recovery.

## Fase 2 — Parser
Expressions, statements, functions, structs, methods, types, imports; AST confiável.

## Fase 3 — AST infrastructure
NodeId, Span, SourceFile, visitor, printer, tests.

## Fase 4 — Binder
Scopes, symbols, declarations, methods, fields, types, imports, exports.

## Fase 5 — Type system
Primitive, named, function, optional; depois inheritance, union, intersection e generics.

## Fase 6 — Roblox database
Hierarchy, services, properties, methods, events, datatypes, enums, availability, docs.

## Fase 7 — Type checker
Assignment, calls, returns, member lookup, casts, optional narrowing, guards, IsA, inheritance.

## Fase 8 — Module system
Import, export, graph, resolution, cycles, dependency tracking.

## Fase 9 — Emitter
Luau AST, printer, constructors, methods, callbacks, signals, source mapping.

## Fase 10 — LSP
Diagnostics, completion, hover, definition, signature, references, rename, formatting.
Protocol: `tower-lsp-server` adapter; semantics stay in `analysis` / `CompilerApi` (see DONT_REINVENT.md).

## Fase 11 — Incremental *(deferred — see DEFERRED.md)*
File versions, dependency graph, query cache, semantic cache, incremental parsing/checking.

## Fase 12 — Cluaupp

Project, build, watch, Roblox, configuration. Compiler hook: `src/platform` + `Session::with_roblox_platform()`.

**Product differentiator (not “more TypeScript”):** DataModel intelligence, context/capability safety, typed remotes, source-level debugging, API refresh, `cluaupp doctor`. See [CLUAUPP_ARCHITECTURE.md](CLUAUPP_ARCHITECTURE.md) and RFC 0007. Killer combo first: Typed DataModel + capabilities + remotes + source maps.

## Fase 13 — High-level opts + benchmarks *(RFC 0011)*

AST Phase 0 shipped: fold / DCE / inline / scalar / loop hoist / mono / `nativeHints`. Cluaupp `@native` selection + fair Numeric/Data/Realistic suites next. Not a second Luau JIT. See [OPTIMIZATION.md](OPTIMIZATION.md).

## Fase 14 — Intent System *(after MVP checker — see INTENT_SYSTEM.md / RFC 0008)*

Comptime reflection, checked generics, contracts, typestate, effects (lite), move-only — then regions / metaclasses. Not a dump of research ideas into `src/` without formal semantics.

## Evitar agora
ISO templates/SFINAE, macros, exceptions, pointers, LLVM, GC e runtime próprio. High-level AST opts **are** allowed (RFC 0011).
