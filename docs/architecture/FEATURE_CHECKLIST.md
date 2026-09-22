# FEATURE_CHECKLIST.md

Para qualquer feature do CL++:

```text
[ ] Grammar
[ ] Token
[ ] AST
[ ] Source spans
[ ] Binder
[ ] Symbol
[ ] Type representation
[ ] Resolver
[ ] Type checker
[ ] Diagnostics
[ ] LSP
[ ] Codegen
[ ] Formatter
[ ] Documentation
[ ] Tests
```

Regra: se uma feature só foi adicionada ao parser, ela ainda não está implementada.

## Matrix (0.8)

| Feature | Grammar | AST | Binder | Types | Checker | Diag | LSP | Emit | Docs | Tests |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Unified TypeId checker | — | — | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ |
| Option / Result | ✓ | ✓ | ✓ | ✓ | ✓ | CLPP0202/1101/1102 | ✓ | ✓ | option-result.md | option_result.rs |
| `?` try | ✓ | Try | ✓ | ✓ | ✓ | CLPP1101 | ✓ | ✓ | ✓ | ✓ |
| Match Ok/Err/Some/None | ✓ | ✓ | ✓ | ✓ | exhaustive | CLPP1102 | ✓ | ✓ | RFC 0013 | ✓ |
| Mono infer | — | — | — | — | — | — | — | opt/mono | RFC 0011 | opt_fold |
| SoA / buffer | — | — | — | — | — | — | — | opt/soa+buffer | OPTIMIZATION | fair_bench |
| Inline cost | — | — | — | — | — | — | — | opt/inline | ✓ | ✓ |
| Escape general | — | — | — | — | — | — | — | opt/escape | ✓ | — |
| Modules cycles | ✓ | ✓ | ✓ | — | ✓ | CLPP1001 | — | — | ✓ | — |
| Platform hooks | — | — | — | — | — | — | — | platform | cluaupp-host | — |
| LSP rename/refs/format | — | — | — | — | — | — | ✓ | fmt | RFC 0005 | — |
| Incremental Session | — | — | — | — | — | — | — | session deps | RFC 0006 | — |
| Contracts / effects / typestate / newtype / move | reserved | — | — | — | — | — | keywords | — | RFC 0014–0018 | — |
| Phase B regions | — | — | — | — | — | — | — | — | RFC 0019 | — |
