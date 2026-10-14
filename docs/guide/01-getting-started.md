# 1. Getting started

The examples on this page are compiled, executed and checked against their displayed output. See [implementation status](../reference/status.md) for the remaining language work.

CL++ is a compiled, general-purpose language designed for games. A `.clp` source file compiles to verified bytecode and runs on the CL++ virtual machine. Each file is a module; there are no header files or required `main()` function.

## Install

Install **[CL++ 0.10.1](https://github.com/KartzRbx/CLPP/releases/tag/v0.10.1)** from GitHub Releases. On Windows x64, install `clpp-language-0.10.1.vsix` through **Extensions → … → Install from VSIX**; it bundles the compiler and runs `clpp --lsp` for IntelliSense. Download `clpp.exe` separately only if you also want the command-line tool on your `PATH`. On Linux or macOS, build from source (below).

To build from source, install CMake 3.20+, Ninja, and a C++20 compiler, then run:

```text
cmake --preset release
cmake --build --preset release
```

Run `./build/release/src/clpp examples/hello.clp` on Linux or macOS, or `.\build\release\src\clpp.exe examples\hello.clp` in PowerShell. Put the executable on `PATH` to invoke `clpp` from any folder. On Linux and macOS, point the VS Code `clpp.serverPath` setting to the built executable or put it on `PATH`.

The current source checkout also supports installing into a chosen prefix and
generating a portable ZIP:

```text
cmake --install build/release --prefix ./build/install --component Runtime
cpack --config build/release/CPackConfig.cmake -B build/distribution
```

The installed CLI is in `bin`, with the license in `share/clpp`. Choose a prefix
you can write to. A Windows portable release must include its required runtime
DLLs or use a toolchain configuration with static C++/thread runtimes; the tested
Windows Release configuration uses static runtimes. Sanitizer builds require
their matching sanitizer runtime and are intended for validation.

## Your first program

Save this as `hello.clp`:

```clp
<< My first CL++ program
post("Hello, CL++!");
```

```output
Hello, CL++!
```

Run `clpp hello.clp`. In VS Code, open the file and press **Ctrl+F5**. Both line and block comments are supported.

## Reading the first line

`post` writes a value to the console. Parentheses enclose the argument, quotation marks make text a string, and `;` ends the statement. `<<` begins a comment, which does not run. Spaces and newlines improve readability; braces group a body. Start with the [language tour](00-language-tour.md), then learn [lexical syntax](02-lexical-syntax.md), [declarations](03-variables-and-types.md), [operators](04-operators.md), and [control flow](05-control-flow.md).

## Program structure

A file can contain `link` imports, declarations (typed functions, `def`, `enum class`), constants, and top-level statements. Top-level statements execute in order in the file passed to `clpp`. Imported files contain declarations and constants only, so importing a module does not run arbitrary code.

```clp
link @clpp.axiom as Axiom;

def Player {
  string name;
  number hp;
}

Player hero = Player { name = "Ada", hp = 90 };
post(hero.name .: ": " .: Axiom.Clamp(hero.hp + 25, 0, 100));
```

```output
Ada: 100
```

## Command line

| Command | Purpose |
| --- | --- |
| `clpp file.clp [args...]` | Compile and run; `args()` returns extra arguments |
| `clpp --repl` | Start the interactive prompt |
| `clpp --lsp` | Start the language server |
| `clpp --version` | Print the version |

Compiler diagnostics identify a file, line, and column so you can find the source location. Runtime errors report the failing operation; see [Diagnostics](../guide/11-errors.md).
