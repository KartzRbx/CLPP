<p align="center">
  <img src="assets/brand/clpp-256.png" width="128" alt="CL++ logo">
</p>

<h1 align="center">CL++</h1>

<p align="center">A compiled, general-purpose programming language designed for games and tools.</p>

CL++ has static typing with inference, value semantics, modules without headers, a bytecode compiler and register virtual machine, and built-in libraries for mathematics, graphics, UI, audio, files, JSON, and automation.

```clp
link @clpp.axiom as Axiom;

struct Player {
  string name;
  int hp;
  Vector3 position;

  func damage(int amount) {
    @hp = Axiom.Clamp(@hp - amount, 0, 100);
  }
}

Player hero = Player("Ada", 100, Vector3(0, 0, 0));
hero.damage(30);
hero.position.y += 2.5;
post(`${hero.name}: ${hero.hp} HP at ${hero.position}`);
```

## Why CL++ exists

Kartz Dev originally wanted a clearer, professional programming experience for Roblox development, inspired by C++ but better suited to that context. CL++ grew into its own general-purpose language with a C++20 implementation, independent modules, a compiler, and a VM. The [about page](https://kartzrbx.github.io/CLPP/guide/00-about/) explains its goals and current uses.

## What you can make

- Games and prototypes with vectors, a native window, 2D drawing, input, audio, and UI.
- Desktop tools with widgets, file I/O, JSON, dates, and a console.
- Visible desktop macros and UI automation on supported platforms.
- Embedded scripts in a C++ application through `clpp_core` and `extern func`.

## Install

**[CL++ 0.10.1](https://github.com/KartzRbx/CLPP/releases/tag/v0.10.1)** is the current release: the C++20 bytecode compiler, register VM, graphics/UI/audio libraries, and the VS Code extension (`clpp-language-0.10.1.vsix`). The extension bundles a self-contained `clpp.exe` for Windows x64, so on Windows there is nothing else to install.

### Install the VS Code extension

Download `clpp-language-0.10.1.vsix` from [Releases](https://github.com/KartzRbx/CLPP/releases), then install it **inside VS Code** — one of:

- In VS Code (or Cursor): open the **Extensions** panel, click the **⋯** menu at the top, choose **Install from VSIX…**, and pick the file; or
- From a terminal: `code --install-extension clpp-language-0.10.1.vsix` (use `cursor` instead of `code` for Cursor).

Then reload the window (**Ctrl+Shift+P → Developer: Reload Window**). Open any `.clp` file to check it works.

> **Do not double-click the `.vsix` file.** On machines with Visual Studio installed, Windows opens `.vsix` with the *Visual Studio* VSIX Installer, which refuses it with “one or more extensions are for Visual Studio Code.” That is a Windows file-association quirk, not a problem with the file — always install from inside VS Code as above.

The extension includes the compiler, so you do not need `clpp.exe` separately on Windows x64. If you want the CLI too, download `clpp.exe` from Releases (or the `clpp-windows` artifact from a successful [CI run](https://github.com/KartzRbx/CLPP/actions/workflows/ci.yml)) and put it on your `PATH`. The released and CI-built `clpp.exe` is statically linked, so it runs on a clean Windows machine with no extra DLLs.

To build from source, install CMake 3.20+, Ninja, and a C++20 compiler:

```text
cmake --preset release
cmake --build --preset release
./build/release/src/clpp examples/hello.clp
```

On Windows, run `.\build\release\src\clpp.exe examples\hello.clp` after the build. Put the executable on your `PATH` to call `clpp` from any directory. On Linux and macOS, point the extension's `clpp.serverPath` setting at the executable or put it on `PATH`.

## Documentation

The [documentation site](https://kartzrbx.github.io/CLPP/) covers the complete language guide, syntax and grammar references, standard library, editor, performance, and project tutorials. Start with [Getting started](https://kartzrbx.github.io/CLPP/guide/01-getting-started/). The [documentation index](docs/README.md) lists repository source material and examples.

## Performance

The compiler verifies bytecode and lowers it to a register VM. Measured optimizations include reusable function frames, fast numeric assignment, indexed reads without copying a whole collection, and specialized integer operations. The [performance chapter](https://kartzrbx.github.io/CLPP/guide/16-performance/) gives the hardware, workloads, timings, and reproduction command. Results are specific to those workloads.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/core/` | Lexer, parser, type checker, code generation, VM, and language server |
| `src/stdlib/` | Built-in modules |
| `src/cli/` | `clpp` executable |
| `include/clpp/` | C++ embedding API |
| `tools/vscode/` | VS Code extension |
| `examples/` | Example programs |
| `benchmarks/` | CL++ and Python benchmark programs |
| `tests/` | Unit, regression, stress, LSP, extension, and documentation tests |
| `docs/` | English guide and reference used to build the website |

## License

See [LICENSE](LICENSE).
