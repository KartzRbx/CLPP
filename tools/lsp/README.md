# tools/lsp

The language server is **`clpp lsp`** (JSON-RPC stdio). This Node file only spawns that binary.

```bash
clpp lsp
node tools/lsp/server.js
```

Do not use `editors/vscode/lsp-server.js` as the IDE entrypoint. The editor pack starts `clpp lsp` through `vscode-languageclient`.
