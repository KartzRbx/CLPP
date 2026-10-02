# 15. Tools and editor

`clpp file.clp [args...]` compiles and runs a program. `clpp --repl` opens an interactive prompt, `clpp --lsp` starts the language server, and `clpp --version` and `--help` print command information. `args()` returns the arguments after the source filename.

## VS Code extension

The CL++ VS Code extension provides syntax highlighting, diagnostics while typing, completion for local and imported members, signature help, hover information, go to definition, references, rename, formatting, semantic colors, and code folding. It also offers a quick fix for assigning to an immutable `let`, file execution with **Ctrl+F5**, and color swatches for `0xRRGGBB` literals.

The extension uses `clpp --lsp`, so its analysis is based on the same compiler as a command-line build. On Windows x64, the VSIX includes `clpp.exe`. The `clpp.serverPath` setting can point to another executable.

## Build and test

```text
cmake --preset release
cmake --build --preset release
ctest --preset debug
```

Presets include `debug`, `release`, `asan`, `tidy`, and Windows `ucrt64`. The test suite checks the lexer, parser, type checker, VM, editor protocol, examples, stress cases, and documentation examples.

## Embedding

The C++20 `clpp_core` library lets a host compile source and run the VM. The host supplies a module loader and can register functions exposed to CL++ as `extern func`. This makes it possible to load scripts from a game's own resource package and direct output to its console.

## Typical workflow

Write a small `.clp` file, run it with `clpp`, and use the editor's diagnostics to fix syntax and type errors. Put reusable declarations in modules, then run the complete program and its tests. Use a release build when measuring performance; use `asan` when investigating C++ memory or undefined-behavior problems in the implementation. When embedding, keep host-specific APIs behind `extern func` declarations so language modules remain easy to test.
