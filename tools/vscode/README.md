# CL++ for Visual Studio Code

Language support for **CL++** (`.clp`), powered by the CL++ compiler itself (`clpp --lsp`): the editor sees exactly what the compiler sees.

## Features

- Syntax highlighting (TextMate grammar + semantic tokens from the compiler)
- IntelliSense: locals, functions, structs, enums, keywords and snippets
- Members after `.`: fields and methods, **including inherited ones**, members of a function's return value, enum variants
- `link` support: `link @clpp.` lists the standard modules, `link "./` lists your `.clp` files, `Alias.` lists everything a linked module exports (functions, structs, enums, constants)
- Hover with signatures and types, go to definition (also into linked files), find references, rename, highlights
- Inline errors while you type (works with Error Lens), quick fix `let` → `let mut`
- Outline, folding, formatting, signature help
- **Run current file** (`Ctrl+F5` or the ▶ button)

## Requirements

The extension ships with `clpp.exe` for Windows x64. On other systems install CL++ and make sure `clpp` is on `PATH`, or set **`clpp.serverPath`**.

## Commands

- `CL++: Run current file`
- `CL++: Restart language server`
- `CL++: Show output`
