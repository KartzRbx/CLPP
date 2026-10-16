The VM extension uses clpp-vm; the independent Rust/Luau clpp executable is never selected automatically. Legacy packages need an explicit serverPath.

# CL++ for Visual Studio Code and Cursor

Language support for **CL++** (`.clp`), powered by the CL++ compiler itself (`clpp-vm --lsp`): the editor sees exactly what the compiler sees.

## Features

- Syntax highlighting (TextMate grammar + semantic tokens from the compiler)
- IntelliSense: locals, functions, structs, enums, keywords and snippets
- Members after `.`: fields and methods, **including inherited ones**, members of a function's return value, enum variants
- `link` support: `link @clpp.` lists the standard modules, `link "./` lists your `.clp` files, `Alias.` lists everything a linked module exports (functions, structs, enums, constants)
- Hover with signatures and types, go to definition (also into linked files), find references, rename, highlights
- Inline errors while you type (works with Error Lens), quick fix to remove `const` from the resolved declaration
- Outline, folding, formatting, signature help
- **Run current file** (`Ctrl+F5` or the ▶ button)

## Requirements

Cursor uses the same VSIX package and settings. Install from VSIX in its Extensions
view, then set `clpp.serverPath` when using a compiler outside PATH. Semantic
highlighting is enabled by default and follows the compiler's resolved roles.
The language registration includes the author's crystal file icon; a selected
third-party icon theme can override language icons.

The extension ships with `clpp-vm.exe` for Windows x64. On other systems install CL++ and make sure `clpp-vm` is on `PATH`, or set **`clpp.serverPath`**.

## Commands

- `CL++: Run current file`
- `CL++: Restart language server`
- `CL++: Show output`

## Local source update 0.10.8

Expression keywords such as `await`, `spawn`, `return` and `thread` are no longer
mistaken for type/function declaration prefixes by the fallback grammar. This
also applies to parenthesized expressions. Theme colors continue to come from
the selected editor theme.

Accepting a module completion inserts its `link` automatically. This works for
project `.clp` files and the native module catalog. Completion details show the
relative path for ambiguous names. Existing imports and visible names take
precedence, and the feature stays out of comments, strings and member lists.
Generated/dependency output directories (`build`, `dist`, `target`, `node_modules`
and hidden directories) are excluded from the bounded filename index. New or
deleted files refresh through editor file notifications or saving a document.

The LSP reuses up to four active analysis graphs across diagnostics and editor
requests. Buffer edits, dependency changes and closed unsaved buffers invalidate
the cache; request timeouts are unchanged.

Format Document respects VS Code's indentation options, keeps loop headers and
initializers together, normalizes generic type brackets and retains inline
comments and paragraph spacing. Consecutive variable declarations stay grouped;
conditionals and loops are separated from adjacent statement groups. The
TextMate grammar recognizes generic types with or without spaces around `< >`.

Formatting preserves the original quotes/escapes in imports, strings and nested
templates. The bundled compiler includes general bounded text/binary reads,
atomic writes, file metadata, canonical/identity paths and executable discovery.

The bundled compiler now releases exited try scopes on return/break/continue and
handles UTF-8 filesystem paths on Windows. Embedded NUL paths are rejected before
filesystem access. Process, path and filesystem APIs support general CLI tools.

The bundled VM supports checked type-first expressions such as `(Outcome "ongoing")`,
including imported aliases, contextual arrays/callbacks and preserved formatter
spacing. Invalid assertions remain errors. Rebuild/package this checkout to use
new APIs; publishing to a marketplace is a separate release action.
