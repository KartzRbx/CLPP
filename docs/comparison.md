---
title: Comparison
description: CL++ next to Luau, roblox-ts, and C++ for Roblox games.
---

# Comparison

CL++ is a language that typechecks and emits Luau. It is not a C++ compiler and it does not run inside Roblox as its own VM.

| | CL++ | Luau | roblox-ts | C++ |
| --- | --- | --- | --- | --- |
| Runs in Roblox | Emits Luau | Native | Emits Luau | Does not run in the Luau VM |
| Module form | `link` (`@clpp`, `@game`, `./`) | `require` | `import` from npm/types | `#include` / modules |
| Incomplete code in the editor | CST keeps the prefix; completion still runs | Depends on the language server | Depends on tsserver | Depends on clangd, and the file must be C++ |
| Client / server mistakes | `CLUAU_AUTH` in the type checker | Convention and code review | Possible with custom types, not a built-in run context | Not a Roblox concept |
| Parallel writes | `CLUAU_PAR` rejects assignment under `@parallel` | Easy to ship a data race | Not a language rule | Data races are a C++ problem, invisible to Roblox |
| DataModel paths | `@game…` from the Rojo project | String paths you maintain | Transformed TS paths | Unrelated |
| Instance lifetime | `~>` plus Janitor, still Luau underneath | Manual `:Connect` / `:Disconnect` | Manual or helper packages | RAII does not map onto Instances |
| Types | Structs, `Option`, `Result`, `?` | Gradual Luau types | TypeScript structural types | Templates, pointers, RAII |
| Host tooling | Cluaupp (Rojo, registry, project) | Rojo, Studio | roblox-ts toolchain | External |

Use Luau when you want the runtime with no other language. Use roblox-ts when the team already thinks in TypeScript. Use C++ off Roblox. Use CL++ when the game source should look like a typed service language and still ship as Luau, with authority and parallel checks in the compiler.
