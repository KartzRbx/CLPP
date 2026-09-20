# tools/lsp

JSON-RPC language server for CL++. The **single entrypoint** is [`editors/vscode/lsp-server.js`](../../editors/vscode/lsp-server.js). This folder’s `server.js` only forwards to that file.

Completions, hover, definition, symbols, format, inlay, and folding call the compiler:

```bash
clpp api complete
clpp api hover
clpp api symbols
clpp api definition
```

The JavaScript catalog (`completions.json`) is fallback for Roblox/Cluaupp types until generated headers fill analysis via `#include`. Do not treat regex `indexDocument` as the type checker.

```bash
node tools/lsp/server.js
```

`clpp install` starts the same server from the editor pack when `clpp.lsp.enabled` is true.

Emitted `.luau` is a different language: install [luau-lsp](https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp). Guide: [CL++ + luau-lsp](../../docs/architecture/luau-lsp.md).
