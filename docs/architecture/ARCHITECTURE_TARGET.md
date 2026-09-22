# Arquitetura alvo

```text
CLPP WORKSPACE
│
├── SOURCE FILES
│    └── Lexer → Parser → AST → Binder → Symbol Table
│                         └────────→ Module Graph
│                                      └→ Type Database
│                                           └→ Type Resolution
│                                                └→ Flow Analysis
│                                                     └→ Type Checker
│                                                          └→ Semantic DB
│                                                               ├→ LSP
│                                                               ├→ Diagnostics
│                                                               └→ Codegen → Luau → Roblox
│
└── ROBLOX API DB ───────────────────────────────→ Symbol / Types

Parallel:
Query / Cache Engine → Parsing / Symbols / Types → Incremental
```
