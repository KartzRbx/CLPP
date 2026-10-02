# Toolchain

CL++ is implemented in C++20. Build it with CMake 3.20+ and a C++20 compiler; Ninja is recommended for incremental builds. The project uses Catch2 for tests, sanitizers for memory and undefined behavior checks, and clang-tidy for static analysis.

## Build presets

```text
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
```

`asan` enables AddressSanitizer and UndefinedBehaviorSanitizer, `tidy` runs clang-tidy during a build, and `ucrt64` targets MSYS2 UCRT64 on Windows. CMake options include `CLPP_BUILD_TESTS`, `CLPP_SANITIZE_ADDRESS`, and `CLPP_ENABLE_CLANG_TIDY`.

## Editor

The CL++ VS Code extension in `tools/vscode/` uses the compiler's language server (`clpp --lsp`) for diagnostics, completion, hover, and navigation. The repository also supports clangd for C++ development through `compile_commands.json`.

See [Tools and editor](../guide/15-tools-and-editor.md) for the CLI, extension features, tests, and embedding API.
