# tools/lsp

JSON-RPC language server for CL++. Reuses the editor pack in `editors/vscode` (`intellisense.js` + `clpp api compile`). No Tree-sitter; Pest in the compiler is the grammar.

```bash
node tools/lsp/server.js
```

stdio, LSP 3.x subset:

- `textDocument/completion` — same IntelliSense as the VS Code pack (`.`, `:`, `~>`, `@this`)
- `textDocument/hover`
- `textDocument/definition` — `#include` paths and `Class::Method` in the workspace
- `textDocument/publishDiagnostics` — pack lint plus `clpp api compile` (debounced)

The VS Code / Cursor pack (`clpp install`) starts this server from `editors/vscode/lsp-server.js` when `clpp.lsp.enabled` is true. If the server fails to start, the pack falls back to in-process providers.

Go-to-definition indexes `.clpp` / `.clp` / `.clh` under the workspace. Workers / progress UI are not used until a real game tree is slow enough to need them.
