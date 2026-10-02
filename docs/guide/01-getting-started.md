# 1. Getting started

CL++ is a compiled, general-purpose language designed for games. A `.clp` source file compiles to verified bytecode and runs on the CL++ virtual machine. Each file is a module; there are no header files or required `main()` function.

## Install

Install **[CL++ 0.10.0](https://github.com/KartzRbx/CLPP/releases/tag/v0.10.0)** from GitHub Releases: `clpp.exe` and `clpp-language-0.10.0.vsix` for Windows x64. Install the VSIX through **Extensions → … → Install from VSIX**; the extension bundles the compiler and runs `clpp --lsp` for IntelliSense. On Linux or macOS, build from source (below) or use a `clpp-windows`-style artifact from a successful [CI run](https://github.com/KartzRbx/CLPP/actions/workflows/ci.yml) when available for your platform.

To build from source, install CMake 3.20+, Ninja, and a C++20 compiler, then run:

```text
cmake --preset release
cmake --build --preset release
```

Run `./build/release/src/clpp examples/hello.clp` on Linux or macOS, or `.\build\release\src\clpp.exe examples\hello.clp` in PowerShell. Put the executable on `PATH` to invoke `clpp` from any folder. On Linux and macOS, point the VS Code `clpp.serverPath` setting to the built executable or put it on `PATH`.

## Your first program

Save this as `hello.clp`:

```clp
<< My first CL++ program
post("Hello, CL++!");
```

Run `clpp hello.clp`. In VS Code, open the file and press **Ctrl+F5**.

## Program structure

A file can contain `link` imports, declarations (`func`, `struct`, `enum`, `variant`, `type`), constants, and top-level statements. Top-level statements execute in order in the file passed to `clpp`. Imported files contain declarations and constants only, so importing a module does not run arbitrary code.

```clp
link @clpp.axiom as Axiom;

struct Player {
  string name;
  int hp;
}

Player hero = Player("Ada", 90);
post(hero.name .: ": " .: Axiom.Clamp(hero.hp + 25, 0, 100));
```

## Command line

| Command | Purpose |
| --- | --- |
| `clpp file.clp [args...]` | Compile and run; `args()` returns extra arguments |
| `clpp --repl` | Start the interactive prompt |
| `clpp --lsp` | Start the language server |
| `clpp --version` | Print the version |

Compiler and runtime diagnostics use `file:line:column: message`, which editors can link to the source.
