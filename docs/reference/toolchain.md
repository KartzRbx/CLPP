# Toolchain

CL++ is implemented in C++20. Build it with CMake 3.20+ and a C++20 compiler; Ninja is recommended for incremental builds. The project uses Catch2 for tests, AddressSanitizer for memory checks, and clang-tidy for static analysis.

## Build presets

`clpp-vm --check script.clp` compiles and verifies a source program without executing it or its native side effects. It returns a nonzero exit status for invalid source; use it when auditing incomplete specification or application examples.

```text
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
```

`asan` enables AddressSanitizer, `tidy` runs clang-tidy during a build, and `ucrt64` targets MSYS2 UCRT64 on Windows. CMake options include `CLPP_BUILD_TESTS`, `CLPP_SANITIZE_ADDRESS`, and `CLPP_ENABLE_CLANG_TIDY`.

## Executable compatibility

The current bytecode format is version 12. Booleans have a distinct runtime kind and CLIR literal tag; malformed boolean payloads are rejected. Async tasks use cooperative scheduling and event operations include checked `Wait` suspension. Method slots are shared across nominal interface views; an absent method occupies the reserved invalid target `65535`, checked at dispatch. Observable registration returns a connection handle. Typed event operations have checked operation/arity operands and shared runtime signal/connection values. Bound method construction has its own checked instruction, retaining the receiver and resolved virtual target. Function records preserve whether indirect invocation must produce a pending async task. CLIR serialization keeps this flag and rejects unknown flags, truncated records and incompatible versions. Older compiled files must be rebuilt with the matching compiler; an installed compiler or extension is not updated by rebuilding this checkout.

## Embedded compiler diagnostics

`CompileResult` and `ClirEmitResult` retain immutable storage for diagnostic
source spans. The spans remain readable after the analyzed source and module
buffers are destroyed, and after copying or moving the result. Keep a result
alive when retaining its diagnostic views; copying a `Diagnostic` alone does
not transfer the result's storage. Successful compilation does not allocate
diagnostic span storage.

## Editor

The CL++ VS Code extension in `tools/vscode/` uses the compiler's language server (`clpp-vm --lsp`) for diagnostics, completion, hover, and navigation. The repository also supports clangd for C++ development through `compile_commands.json`.

The LSP validates JSON-RPC field types before dispatch. Malformed JSON and request
envelopes receive protocol errors; subsequent valid requests remain usable.
Transport headers are limited to 16 KiB and message bodies to 64 MiB. Invalid,
duplicated, negative, overflowing or excessive Content-Length headers terminate
the stream without attempting the advertised allocation. A client should open a
new server connection after a transport failure.

## Native validation builds

The 2026-10-14 local validation checkpoint passed 512 integrated checks with
GCC on Windows and the same 512 checks under Windows LLVM MinGW ASan + UBSan.
Release hostile-input testing passed 1,052 cases; the instrumented corpus passed
302 cases. These finite tests are evidence for those builds, not a proof that all
vulnerabilities are absent. Leak detection, Linux/macOS/MSVC runners and visual
editor verification are separate gates. No new release or website deployment
is implied by a local build.

The `asan` preset enables AddressSanitizer and UndefinedBehaviorSanitizer on a
Clang/GCC toolchain with the corresponding runtimes. Undefined-behavior reports
stop execution. MSVC supports the separate address-sanitizer option; it does not
support this combined preset. The CI matrix executes normal tests in both Debug
and Release, plus a bounded malformed-input matrix. Platform and sanitizer gates
are complete only after those binaries have actually built and their tests run.

See [Tools and editor](../guide/15-tools-and-editor.md) for the CLI, extension features, tests, and embedding API.
