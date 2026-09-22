# COMPILER_ARCHITECTURE.md

## Fluxo-alvo

CL++ source → Pest (parser) → AST → Binder → Symbols → Modules → Types → Checker → Semantic Model

Do semantic model saem:

- IntelliSense
- Hover
- Refactor
- Diagnostics

Depois:

Semantic Model → Lowering → Luau AST → Printer → Luau → Roblox

## Componentes (código atual)

```text
src/
  parser/          Pest PEG (no separate lexer)
  ast/
  binder/
  symbols/
  types/
  checker/
  names.rs         Instance / global tables + luau_type (ex-semantic helpers)
  modules/
  diagnostics/     facade → diag.rs
  emitter/         facade → codegen
  driver/          facade → compile + session
  platform/        optional Roblox prelude
  roblox/
  session/
  analysis/
  lsp/

tests/
rfc/
docs/architecture/
```

## Regra arquitetural
O parser não faz type checking. O AST não depende de Roblox. O LSP consome o modelo semântico e não conhece os internals do compilador.

## Incrementalidade
Versionamento de arquivos, dependency graph, query cache, semantic cache, parsing incremental e checking incremental entram depois que a base semântica estiver estável. Ver [DEFERRED.md](DEFERRED.md) e RFC 0006.
