# About CL++

## Why CL++ was created

Kartz Dev wanted a language closer to the needs of software development while retaining a professional structure. The initial idea was to bring a C++-like programming experience to Roblox development, but C++ syntax and workflow did not fit that setting well. CL++ began as an original language with clearer syntax and the capabilities needed for serious programs. Its current implementation is a general-purpose compiler and virtual machine with game-oriented libraries.

## What you can build

- **Games and prototypes:** vectors, math helpers, frame loops, drawing, input, sound, and UI are available from built-in modules.
- **Desktop tools:** use windows, widgets, JSON, files, and console I/O for editors, utilities, and data tools.
- **Automation:** drive keyboard and mouse actions, inspect screen pixels, and combine them with file and JSON operations.
- **Embedded scripting:** a C++ host can compile CL++ modules, supply a module loader, and expose host functions.
- **Learning and experimentation:** readable syntax, explicit control flow, a REPL, and editor diagnostics make small programs easy to inspect.

## How it works

The compiler reads `.clp` modules, checks names and types, emits verified bytecode, and runs it on a register virtual machine. The language supports type inference, value semantics, structs with inheritance, pattern matching, tasks, events, and a standard library. The compiler also powers the VS Code language server, so editor feedback uses the same analysis as command-line compilation.

CL++ aims to make common game and tool code concise without hiding control flow or type errors. Use value types when independent copies are useful; use `move` for large values you want to transfer. Use the built-in graphics and UI modules for self-contained apps. For a workload needing native libraries or engine integration, embed the core library and expose host functions through `extern func`.

## Performance and reliability

The VM reuses function frames, handles numeric values efficiently, avoids unnecessary copies for indexed reads, and specializes integer arithmetic. Bytecode verification, bounds checks, recursion limits, and sanitizer testing address common failures. See [Performance](16-performance.md) for measured workloads, hardware, and reproduction steps; the published numbers describe specific benchmarks rather than a universal speed comparison.

Start with [Getting started](01-getting-started.md), then use the rest of the guide as a path from syntax to real applications.
