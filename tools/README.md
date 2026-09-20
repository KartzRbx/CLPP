# tools/

Tooling around the language. The compiler binary owns the real commands.

- `fmt/` — `clpp fmt` / `clpp api format`
- `lsp/` — thin stdio wrapper around `editors/vscode/lsp-server.js` (`clpp api complete`)
- `repl/` — no VM; compile and run Luau instead
