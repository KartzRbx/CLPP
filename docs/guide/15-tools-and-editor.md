# 15. Tools and editor

## Zed colors and optional presence

The local Zed extension supplies syntax and compiler semantic highlighting,
completion and hover. Its installer snapshots the compiler separately from build
outputs and preserves additional language servers when it is rerun.

Hexadecimal color swatches come from `documentColor`; replacement hexadecimal
edits come from `colorPresentation`. A clickable visual color picker depends on
the editor. Zed's [color-picker request](https://github.com/zed-industries/extensions/issues/495)
remains open; the swatch does not imply that this UI exists. Edit the hexadecimal
directly in Zed. The VS Code extension registers a color provider for clients
that support an interactive picker.

Optional Discord Rich Presence uses a separate local Zed adapter and the
existing Discord Presence extension in VS Code/Cursor. It displays CL++ project
details using the editor's public application ID, without a user token or Client
Secret. It starts disabled until sharing workspace/file names with Discord is
approved. This is prepared local integration; a displayed public profile has not
been verified, and it is not part of compiler correctness.

`clpp-vm file.clp [args...]` compiles and runs a program. `clpp-vm --repl` opens an interactive prompt, `clpp-vm --lsp` starts the language server, and `clpp-vm --version` and `--help` print command information. `args()` returns the arguments after the source filename.

## VS Code extension

These capabilities describe the current source checkout's extension and compiler.
An older installed release may require an update. Proposed and host-specific
forms remain separate from implemented language contracts. See
[implementation status](../reference/status.md).

The CL++ VS Code extension provides syntax highlighting, diagnostics while typing, completion for local and imported members, signature help, hover information, go to definition, references, rename, formatting, semantic colors, and code folding. It also offers file execution with **Ctrl+F5**, and color swatches for `0xRRGGBB` literals.

The extension uses `clpp-vm --lsp`, so its analysis is based on the same compiler as a command-line build. On Windows x64, the VSIX includes `clpp-vm.exe`. The `clpp.serverPath` setting can point to another executable.

Member completion includes accessible inherited fields and methods, and named
initializers offer inherited fields without suggesting private fields or methods.
An explicitly typed callback parameter has the same members as an ordinary
variable of that type. Generic object members show their concrete types, such as
`number` for the value of `Box<number>`.

```clp
def Named {
  string Name = "Kartz";
  string Read() { return @this.Name; }
}
def Player : Named {
  signal<number> Changed = signal<number>();
}
Player player = Player {};
signal<Player> joined;
connection handler = joined::Connect(func(Player person) -> void {
  post(person.Read());
  person.Changed::Fire(15);
});
joined::Fire(player);
handler::Disconnect();
```

```output
Kartz
```

In this example, `person.` offers `Name`, `Read` and `Changed`, and
`person.Changed::` offers the signal operations. Completing `Connect` after the
first `:` inserts the second colon and an explicitly typed callback. The type
test keeps its separate `value:is<Type>` spelling.

**Format Document** uses the editor's tab width and spaces/tabs preference.
It keeps `for` headers together, attaches initializer punctuation, normalizes
generic brackets such as `array<T>` and separates statement groups with blank
lines. Consecutive variable declarations stay grouped. Original literal contents
and inline comments are preserved; input with malformed literals is left alone.
The VS Code grammar recognizes generic types even before spacing is normalized.

References and Rename currently cover variables and parameters in the current
document, including captured references and shadowed names. Rename rejects
reserved words, invalid identifiers and changes that would capture another
binding. Renaming fields, types, methods or symbols across a workspace is not
provided by this refactoring interface. Reference results honor the editor's
option to include or exclude the declaration.

Open-buffer changes use increasing document versions. Older notifications and
malformed edit ranges cannot replace the current buffer. UTF-16 positions are
used for editor edits and navigation, including text after emoji. The synchronous
server safely ignores late cancellation notifications; it does not interrupt an
analysis already running.

## Import suggestions

Type a module filename's base name, select its completion and accept it. For
example, accepting `Alerts` can insert `link "./lib/Alerts.clp" as Alerts;` above
your code. Native modules offer the same behavior, such as `Json` inserting
`link @clpp.json as Json;`. This uses ordinary `link` syntax and works for project
`.clp` modules generally, without an application-specific module list.

Suggestions include the source path, so equal names in different directories
remain distinguishable. Existing imports are reused; local names take priority.
Comments, strings and member access do not trigger new imports. The filename
index excludes generated output and hidden directories and refreshes when the
editor reports file creation/deletion or saves a document. Files with separators
in their names get a valid alias, for example `notice-tools.clp` becomes
`notice_tools`.

## Build and test

```text
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
```

Presets include `debug`, `release`, `asan`, `tidy`, and Windows `ucrt64`. The test suite checks the lexer, parser, type checker, VM, editor protocol, examples, stress cases, and documentation examples.

## Embedding

The C++20 `clpp_core` library lets a host compile source and run the VM. The host supplies a module loader. The target spelling and availability of general native function registration still need compiler alignment; the current parser only supports its restricted native declaration path. This makes it possible to load scripts from a game's own resource package and direct output to its console.

## Typical workflow

Write a small `.clp` file, run it with `clpp`, and use the editor's diagnostics to fix syntax and type errors. Put reusable declarations in modules, then run the complete program and its tests. Use a release build when measuring performance; use `asan` when investigating C++ memory or undefined-behavior problems in the implementation. When embedding, keep host-specific APIs behind host function interfaces so language modules remain easy to test.
