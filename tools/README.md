# Editor integrations

CL++ uses one compiler-backed language server: `clpp-vm --lsp`, with JSON-RPC over
standard input/output and standard Content-Length framing. Editors use language
ID `clpp` for `.clp`. Syntax colors and file icons are separate editor assets.

| Editor | Integration | Validation here |
| --- | --- | --- |
| VS Code | `vscode/` VSIX, TextMate grammar, compiler semantic tokens and file icon | Provider integration tests against the real compiler |
| Cursor | Same VSIX and `clpp.serverPath` setting as VS Code | Shared adapter tests; no live Cursor installation test |
| Zed | `zed/` WASI extension, structural Tree-sitter grammar, compiler semantic tokens and icon theme | Built/installed locally; parsing, capture roles, queries and LSP tests |
| Other LSP-capable editors | Configure executable `clpp` with argument `--lsp`, stdio transport and `.clp` file association | Protocol support; individual editor adapters not tested here |

The server offers diagnostics, completions, hover, signature help, definitions,
references, rename, document symbols, formatting, folding and semantic tokens.
Clients choose which protocol features to display. This is not a claim that
every IDE has an installed extension: editors without configurable LSP support
need a dedicated adapter. No editor-specific language implementation is used.

## Common configuration

- Executable: absolute path to the current `clpp` / `clpp-vm.exe`, or `clpp` on PATH.
- Arguments: `["--lsp"]`.
- Transport: stdio; do not launch through a shell that prints banners on stdout.
- File extension: `.clp`; language identifier: `clpp`.
- Workspace root: project directory, to resolve and watch source modules.
- Semantic tokens: enable them alongside the editor's syntax grammar.

Windows UCRT64 development builds require `C:\msys64\ucrt64\bin` on the server
process PATH. Release builds must supply their runtime dependencies. Zed's local
installer provides this environment; other editors must inherit it or configure it.

## Portable VS Code/Cursor package

```powershell
python tools/vscode/package_vsix.py --out build/clpp-language-local.vsix
```

Use **Extensions: Install from VSIX** in VS Code or Cursor and select that file.
This package has no bundled compiler: configure `clpp.serverPath` to the compiler
you built and provide its DLL environment. `--exe` packaging is intended for
self-contained release binaries, not a promise to bundle dependencies automatically.

See [Zed setup](zed/README.md) and [VS Code/Cursor setup](vscode/README.md).
