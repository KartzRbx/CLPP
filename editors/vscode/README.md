# editors/vscode

VS Code language pack for CL++. Works in **VS Code**, **Cursor**, **VS Code Insiders**, **VSCodium**, and **Windsurf** (`vscode` API).

```bash
clpp install
clpp install --editor code
clpp install --editor cursor
```

Includes `.clpp` / `.clp` / `.clh` (and `.flare` `.mint` `.bloom` `.helm` `.shift` `.hive` `.axiom`) identification, icon, TextMate highlighting, snippets, IntelliSense, and **`clpp lsp`** via `vscode-languageclient`. Set `clpp.lsp.enabled` to false to stay in-process.

Typing `@`, `.`, or any identifier completes from memory: keywords, builtins, the current buffer, and include types already in the cache. Disk walks and `clpp api compile` never run on the completion or semantic-token path. Includes load in the background after a 250ms pause.

`@` and `@this` use the same keyword scope (`keyword.other.receiver.clpp`). `@janitor` keeps that keyword on `@` and colors the field as `variable.other.property.receiver.clpp`. Hover still splits meaning: `@this` → `self`, `@field` → `self.field`.
