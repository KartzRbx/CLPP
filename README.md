<p align="center">
  <a href="https://kartzrbx.github.io/CLPP/">
    <img src="assets/brand/clpp-256.png" width="144" height="144" alt="CL++ logo">
  </a>
</p>

<h1 align="center">CL++</h1>
<p align="center"><strong>Build playable prototypes and desktop tools with one language.</strong><br>Static types, header-free modules, built-in graphics, UI, and audio.</p>

<p align="center">
  <a href="https://kartzrbx.github.io/CLPP/">Documentation</a> ·
  <a href="https://github.com/KartzRbx/CLPP/releases">Download</a> ·
  <a href="https://kartzrbx.github.io/CLPP/guide/01-getting-started/">Get started</a> ·
  <a href="https://kartzrbx.github.io/CLPP/reference/syntax/">Language reference</a> ·
  <a href="examples/">Examples</a> ·
  <a href="https://github.com/KartzRbx/CLPP/issues">Issues</a>
</p>

| 🚀 Start coding | 📖 Look up syntax | 🧰 Explore the library |
| :--- | :--- | :--- |
| [**Getting started**](https://kartzrbx.github.io/CLPP/guide/01-getting-started/)<br>Install, write a `.clp` file, and run it. | [**Keywords & operators**](https://kartzrbx.github.io/CLPP/reference/keywords/)<br>Quick reference for the language. | [**Standard library**](https://kartzrbx.github.io/CLPP/guide/14-standard-library/)<br>Math, files, JSON, and more. |
| [**Editor setup**](https://kartzrbx.github.io/CLPP/guide/15-tools-and-editor/)<br>Completion, diagnostics, and navigation. | [**Grammar**](https://kartzrbx.github.io/CLPP/reference/ebnf/)<br>Exact syntax in EBNF. | [**Graphics & windows**](https://kartzrbx.github.io/CLPP/guide/17-window-and-graphics/)<br>Draw your first interactive scene. |

## <img src="assets/brand/clpp-32.png" width="28" height="28" alt=""> Why build with CL++?

- **Get something on screen:** windows, drawing, input, UI, and audio are built in.
- **Keep game code organized:** typed data, functions, and independent `.clp` modules — no headers or required `main()`.
- **Catch mistakes while you work:** compiler-powered diagnostics, completion, hover, and navigation in VS Code. On Windows x64, the extension includes the compiler.

[See a complete game system](https://kartzrbx.github.io/CLPP/guide/20-modular-combat/) · [Build an app screen](https://kartzrbx.github.io/CLPP/guide/21-account-screen/) · [Explore measured performance](https://kartzrbx.github.io/CLPP/guide/16-performance/)

> **Language status:** the site now describes the author's target syntax. Some constructs still need compiler alignment. [Complete language handbook](https://kartzrbx.github.io/CLPP/handbook/00-o-que-e-o-clpp/) · [Learn the syntax](https://kartzrbx.github.io/CLPP/guide/00-language-tour/) · [Implementation status](https://kartzrbx.github.io/CLPP/reference/status/)

## Hello, CL++

Save as `hello.clp`:

```clp
post("Hello, CL++!");
```

Run `clpp hello.clp`, or open the file in VS Code and press **Ctrl+F5**.

## <img src="assets/brand/clpp-32.png" width="28" height="28" alt=""> Install

Download the `.vsix` from [**Releases**](https://github.com/KartzRbx/CLPP/releases). In VS Code or Cursor, select **Extensions → ⋯ → Install from VSIX…**, then reload the window. The extension bundles the compiler for **Windows x64**.

<details>
<summary><strong>CLI, Linux/macOS, and building from source</strong></summary>

For the Windows CLI, download `clpp.exe` from Releases and add its folder to `PATH`. The executable is statically linked and needs no extra DLLs.

To build from source, install **CMake 3.20+**, **Ninja**, and a **C++20 compiler**:

```sh
cmake --preset release
cmake --build --preset release
```

Run `./build/release/src/clpp examples/hello.clp` on Linux/macOS, or `.\build\release\src\clpp.exe examples\hello.clp` in PowerShell. On Linux/macOS, put the compiler on `PATH` or set the extension's `clpp.serverPath`.

[Toolchain reference](https://kartzrbx.github.io/CLPP/reference/toolchain/) · [CI builds](https://github.com/KartzRbx/CLPP/actions/workflows/ci.yml)

</details>

<details>
<summary><strong>Windows opens the wrong VSIX installer?</strong></summary>

Install the `.vsix` **from inside VS Code**. Windows may associate `.vsix` with Visual Studio's installer. Alternatively, run `code --install-extension <downloaded-file>.vsix` (use `cursor` for Cursor).

</details>

## A small example

```clp
link @clpp.axiom as Axiom;

def Player {
  string name;
  number hp = 100;
}

Player hero = Player { name = "Ada", hp = 90 };
hero.hp = Axiom.Clamp(hero.hp + 25, 0, 100);
post(hero.name .: ": " .: hero.hp .: " HP");
```

```output
Ada: 100 HP
```

Expected output after compiler alignment: `Ada: 100 HP`.

[Types & variables](https://kartzrbx.github.io/CLPP/guide/03-variables-and-types/) · [Data definitions](https://kartzrbx.github.io/CLPP/guide/07-structs-and-inheritance/) · [Modules](https://kartzrbx.github.io/CLPP/guide/10-modules/)

<details>
<summary><strong>Try a window with graphics</strong></summary>

Save as `window.clp` and run `clpp window.clp`:

```clp
link @clpp.gfx as Gfx;
link @clpp.window as Window;

Window.Open("Hello", 480, 320);
while (Window.Frame()) {
  Gfx.Clear(Gfx.DARK);
  Gfx.Text("Hello, CL++!", 40, 40, 24, Gfx.WHITE);
  Gfx.Circle(240, 200, 60, Gfx.ORANGE);
}
```

[Graphics guide](https://kartzrbx.github.io/CLPP/guide/17-window-and-graphics/) · [UI guide](https://kartzrbx.github.io/CLPP/guide/18-user-interfaces/)

</details>

## <img src="assets/brand/clpp-32.png" width="28" height="28" alt=""> Build something

| 🎮 Games | 🖥️ Tools | 🧩 Complete projects |
| :--- | :--- | :--- |
| [Graphics & input](https://kartzrbx.github.io/CLPP/guide/17-window-and-graphics/)<br>[Events](https://kartzrbx.github.io/CLPP/guide/13-events/)<br>[Audio](https://kartzrbx.github.io/CLPP/guide/19-audio-console-automation/) | [User interfaces](https://kartzrbx.github.io/CLPP/guide/18-user-interfaces/)<br>[Files & JSON](https://kartzrbx.github.io/CLPP/guide/14-standard-library/)<br>[Concurrency](https://kartzrbx.github.io/CLPP/guide/12-concurrency/) | [Modular combat](https://kartzrbx.github.io/CLPP/guide/20-modular-combat/)<br>[Account screen](https://kartzrbx.github.io/CLPP/guide/21-account-screen/)<br>[Example sources](examples/) |

<details>
<summary><strong>About the language, performance, and repository</strong></summary>

CL++ started with Roblox development and grew into a general-purpose language. The current implementation uses **C++20**, verified bytecode, and a register VM. Each `.clp` file is a module; there are no headers or required `main()` function.

[About CL++](https://kartzrbx.github.io/CLPP/guide/00-about/) · [Measured benchmarks](https://kartzrbx.github.io/CLPP/guide/16-performance/)

| Path | Contents |
| --- | --- |
| `src/core/` | Lexer, parser, checker, bytecode, VM, and language server |
| `src/stdlib/` | Built-in modules |
| `src/cli/` | `clpp` executable |
| `include/clpp/` | C++ embedding API |
| `tools/vscode/` | VS Code extension |
| `examples/`, `benchmarks/`, `tests/` | Programs, benchmarks, and validation |
| `docs/` | Website guide and reference sources |
| `www/` | Astro / Starlight website |

Website commands run from `www/`:

```sh
npm ci
npm run dev
npm run build
```

The build generates pages and navigation from `docs/` and writes the static site to `www/dist/`. Source documents are listed in the [documentation index](docs/README.md).

</details>

Licensed under [MIT](LICENSE).
