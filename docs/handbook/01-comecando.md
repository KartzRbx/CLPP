# Getting started

This handbook teaches CL++'s target syntax. **CL++ officially compiles to bytecode and runs on its own VM.** Roblox and Luau are possible domains for a transpiler you write in CL++; they are not an official backend. Check [compiler status](../reference/status.md) before running target examples.

## Install the current executable

On Windows x64, obtain the VSIX extension from [GitHub Releases](https://github.com/KartzRbx/CLPP/releases). In VS Code, use **Extensions → … → Install from VSIX**. The extension includes the compiler; `clpp.serverPath` can select another executable. Put `clpp-vm.exe` on your `PATH` if you want to use it from a terminal.

To build the current source, install CMake 3.20+, Ninja, and a C++20 compiler:

```text
cmake --preset release
cmake --build --preset release
```

Check installation with `clpp-vm --version`. Use the current release's instructions rather than assuming a Rust build, setup command, or separate installer.

## Your first program

Create `hello.clp`:

```clp
post("hello, world");
```

```output
hello, world
```

`clpp-vm hello.clp` compiles and executes on the VM. This isolated `post` call also works with the current executable; this handbook's `target` marking keeps specification examples separate from executable tests.

A named function organizes reusable behavior, but naming one `init` does not automatically run it:

```clp
void init() {
    post("hello, world");
}
init();
```

```output
hello, world
```

The entry file's top-level statements execute in order. There is no mandatory `main()` or special `init()` entry function. A host application can choose its own lifecycle convention; that does not change the language's ordinary call rules.

## Files and modules, without headers

**Declarations and implementation belong in the same source module. CL++ does not use headers.**

| File | Role |
| --- | --- |
| `*.clp` | CL++ source module connected through `link` |
| `*.clpp` | A possible source-file convention for a user-written transpiler; not a promised Roblox mapping |
| `*.server.clpp` | A possible server-context input convention for that application |
| `*.client.clpp` | A possible client-context input convention for that application |

Create a `.clp` module when other parts of the program need its data or functions. Use `link` to connect it. Public contracts and implementations share a source file. Use `export` before a declaration to expose it; declarations without `export` are internal. Paths resolve relative to the importing file.

A custom Roblox transpiler could interpret suffixes and generate ModuleScripts, Scripts, or LocalScripts. It would have to define those mappings and enforce its own [context rules](./10-contextos-e-seguranca.md). A suffix alone does not supply engine APIs or official compiler support.

## Comments

```clp
<< One line of commentary.
<< Another line of the same explanation.
post("line comments");
```

```output
line comments
```

Block comments use `<<[ ... ]>>` and can span several lines; their lexical behavior is tested.

```clp
<<[ A comment
   spanning multiple lines. ]>>
post("block comments");
```

```output
block comments
```

## Commands available now

| Command | Purpose |
| --- | --- |
| `clpp-vm file.clp [arguments...]` | Compile and execute on the VM; `args()` reads extra arguments |
| `clpp-vm --repl` | Open the interactive prompt |
| `clpp-vm --lsp` | Start the language server used by the editor |
| `clpp-vm --version`, `clpp-vm --help` | Read version and command help |

## Tooling ideas from the supplied design

The original handbook described the following interfaces. They are **design ideas, not available commands or an official Luau roadmap**. A separate compiler tool or a transpiler application could implement them under its own command interface.

| Proposed interface | Intended responsibility |
| --- | --- |
| `compile file` | Generate output for one input file |
| `build` | Process an entire project |
| `watch` | Rebuild after changes |
| `fmt file` | Format source |
| `manifest` | Expose file conventions, operators, and builtins to tools |
| `api compile` | Provide structured JSON compilation input/output |
| `api complete`, `hover`, `symbols`, `definition` | Provide structured editor operations |

Current CL++ editor integration uses `clpp-vm --lsp`; an application-specific JSON API does not replace that implemented interface.

## A practical workflow

Write a `.clp` program compatible with the installed compiler, run `clpp-vm file.clp`, and use editor diagnostics. Target examples await the language migration and should not be mistaken for release compatibility tests.

For a **user-written Roblox transpiler project**, you would define its input syntax and context rules, emit Luau, and synchronize that application's output with Studio. Rojo or Cluaupp could handle synchronization if your project integrates them. Implementing a transpiler and inserting its output into Studio are separate responsibilities; see [Roblox and Cluaupp](./13-roblox-e-cluaupp.md).
