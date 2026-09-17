---
title: Install the compiler
---

# Install the compiler

CL++ ships as a Rust binary named `clpp` plus an editor language pack (icon, highlighting, IntelliSense).

## 1. Compiler

From the repository:

```bash
cargo install --path .
```

Check:

```bash
clpp --help
```

You should see `compile`, `build`, `api`, `install`, and `manifest`.

## 2. Editor pack

```bash
clpp install
```

This copies the pack into **VS Code** and **Cursor** (and Insiders, VSCodium, Windsurf when those products exist). Reload the window. Files `.clpp`, `.clp`, and `.clh` get the CL++ icon.

Target one editor:

```bash
clpp install --editor cursor
clpp install --editor code
```

## 3. Compile a file

```bash
clpp compile examples/hello/hello.server.clpp
clpp compile examples/hello/hello.server.clpp -o out/hello.server.luau
clpp build examples -o out
```

JSON (Cluaupp contract):

```bash
clpp compile examples/hello/hello.server.clpp --json
clpp api compile --file examples/hello/hello.server.clpp
clpp api manifest
```

## This documentation site

```bash
npm run docs
```

That runs Moonwave (`moonwave dev --code moonwave`) and opens the course plus the generated API.

Next: [Your first script](hello-world).
