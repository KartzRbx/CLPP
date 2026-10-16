# CL++ for Zed

Local Windows integration for `.clp`: syntax colors, comments, bracket matching,
indentation and the compiler's language server. Completion, hover, diagnostics,
definitions, references, rename, symbols and formatting use `clpp-vm --lsp`.

The editor grammar distinguishes declarations, types, parameters, calls, stored
generic function values, `decltype`, members, generic arguments, expressions and
control flow. It does not infer types from
capitalization. Compiler semantic tokens supplement the syntax with resolved
bindings and function values; the local installer enables `semantic_tokens: combined`.
Type checking always belongs to the CL++ language server.
Comments use `<<` through the end of the line and `<<[ ... ]>>` across lines.
The grammar's stateless scanner consumes each whole comment, so code-like text
inside a comment cannot be classified as functions, keywords or types.
The package uses Tree-sitter ABI 15, supported by current Zed versions; this is
also needed for global reserved words in the grammar.
The receiver expression is `@this`; `self` is not highlighted as a language keyword
or special receiver. A programmer-defined name is classified as an ordinary binding.
This extension is currently local and has not been published in Zed's registry.

## Build and install on this development machine

From the repository root, after building `build/ucrt64/src/clpp-vm.exe`:

```powershell
rustup target add wasm32-wasip2
powershell -ExecutionPolicy Bypass -File tools/zed/build.ps1 -WasiSdkPath build/wasi-sdk-34.0-x86_64-windows
node tools/zed/install-local.cjs
```

The build requires Node/npm, Rust and the official WASI SDK. It produces
`build/zed-extension/clpp`, including both WebAssembly binaries. The local
installer copies that prebuilt package into `%LOCALAPPDATA%/Zed/extensions/installed/clpp`,
backs up `%APPDATA%/Zed/settings.json` and merges only CL++ settings through a
JSONC parser. It preserves other settings and their comments. Restart Zed or
reload extensions and open `.clp`; the language picker should show **CL++**.

The installer snapshots this checkout's compiler into a content-addressed
directory in the installed extension and preserves its configured environment.
The production Windows runtime is statically linked and needs no compiler SDK
directory on PATH. Instrumented development builds require their matching runtime.
Rebuilding the checkout does not replace or require stopping the live server.
Reinstall to select a newly built compiler. For a separate build/package use
`node tools/zed/install-local.cjs --package <directory> --compiler <executable>`.
A different installation can set `lsp.clpp.binary.path`, `arguments`
(`--lsp`) and `env` in Zed settings; with no binary override, the extension finds
`clpp-vm` on PATH. Rebuild CL++ and rerun the installer to update language features without rebuilding the
extension. Run the installer again after moving the checkout.

The grammar source must be committed and its revision pinned before registry
publication or installation through Zed's **Install Dev Extension** builder;
the prebuilt local package does not fetch grammar sources.

## Validation

```powershell
cargo check --manifest-path tools/zed/Cargo.toml --locked
cd tools/zed/grammar
npm run generate
node_modules/.bin/tree-sitter parse -q smoke.clp
node_modules/.bin/tree-sitter query ../languages/clpp/highlights.scm smoke.clp
```

Compiler protocol tests are in `tests/lsp/test_lsp.py`. Successful package and
protocol checks do not by themselves confirm the editor loaded the extension;
check Zed's log after installation for grammar or language-server errors.
`tests/zed/test_highlights.py` checks Combat's reported function/type/parameter/member
roles, compiler semantic tokens, all queries and parsing of every `.clp` source in
`examples/` and `benchmarks/`, plus the `@this` editor fixture, the reported
inheritance comment heading and invalid syntax. Run with the UCRT64 DLL directory on PATH.

The extension registers the author's crystal icon for `.clp` using an SVG container
with the original PNG embedded unchanged. The local installer extends the selected
icon theme as `<existing theme> + CL++`, copying its assets locally and leaving the
original theme extension intact. Reinstalling recreates this local derivative.
The portable package also includes a minimal **CL++ Icons** theme.

Hexadecimal colors (`0xRRGGBB` and packed `0xTTRRGGBB`, where TT is transparency)
are provided by the shared language server through `documentColor`. Strings and
comments do not produce literal previews. The installer enables
`languages.CL++.lsp_document_colors: "inlay"` for color swatches beside the literals.
The server also implements `colorPresentation` for clients with a color picker.
Zed rendering settings are documented in [LSP document colors](https://zed.dev/docs/reference/all-settings#lsp-document-colors).

Official integration references: [language extensions](https://zed.dev/docs/extensions/languages)
and [extension development](https://zed.dev/docs/extensions/developing-extensions).
