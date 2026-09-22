---
title: Install CL++
---

# Install CL++

Download the installer, run it, and CL++ is on your machine: the `clpp` compiler, editor highlighting, IntelliSense, and a language server.

<div class="clpp-download">

[Download CL++ installer for Windows](https://github.com/KartzRbx/CLPP/releases/latest/download/clpp-setup.exe)

</div>

1. Save `clpp-setup.exe`.
2. Double-click it. The installer copies the compiler to `%LOCALAPPDATA%\Programs\CLPP`, adds it to your PATH, and installs the language pack in Cursor and VS Code.
3. Open a **new** terminal and run `clpp --help`.
4. Reload the editor window.

That is the full path: the site gives you the installer; the installer installs the language.

## From the command line

If you already have `clpp`:

```bash
clpp setup
```

That replaces an older compiler on this machine (for example `0.1.0`) with the one you just ran, then copies the matching editor pack.

Editor pack only (compiler already installed):

```bash
clpp install
clpp install --editor cursor
clpp install --editor code
```

## From source

```bash
cargo install --path .
clpp setup
```

## Compile a file

```bash
clpp compile examples/syntax/features.clp
clpp build examples -o out
clpp fmt examples/syntax/features.clp
clpp watch examples -o out
```

Next: [Your first script](hello-world).
