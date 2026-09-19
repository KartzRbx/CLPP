# tools/lsp

JSON-RPC language server for CL++. Reuses the editor pack in `editors/vscode` (`intellisense.js` + `clpp api compile`). No Tree-sitter; Pest in the compiler is the grammar.

```bash
node tools/lsp/server.js
```

stdio, LSP 3.x subset:

- `initialize` returns capabilities only (`@` is a completion trigger). Workspace walk and `clpp api compile` do not run here.
- `initialized` starts a worker thread (`index-worker.js`) to index `.clpp` / `.clp` / `.clh`. Skips `node_modules`, `target`, `.git`, `build`, `dist`, `www`, `.odr`, and other cache dirs.
- `textDocument/completion` — keywords, `@this` / `@janitor`, and the current buffer. Include types come from an in-memory cache filled in the background. Completions never `existsSync` / `readFileSync`.
- `textDocument/hover`
- `textDocument/definition` — `#include` paths and `Class::Method` from the worker index
- `textDocument/publishDiagnostics` — pack lint plus async `clpp api compile` (`spawn`, 250ms debounce)

The VS Code / Cursor pack (`clpp install`) starts this server from `editors/vscode/lsp-server.js` when `clpp.lsp.enabled` is true. Completions register in-process on activate and do not wait for the handshake. If the server fails to start, diagnostics fall back in-process (also async).
