# CL++

**CL++** is a C++-inspired programming language that compiles to [Luau](https://luau.org). The compiler is `clpp`, written in Rust.

CL++ is the **language** — syntax, semantics, and OOP. [Cluaupp](https://github.com/KartzRbx/Cluaupp) owns Roblox API wiring (generated headers, Rojo, project `init`).

## File extensions

| Extension | Role |
| --- | --- |
| `.clh` | Header — `struct`, constants, prototypes |
| `.clp` | Implementation / module |
| `.clpp` | CL++ implementation (Scripts, LocalScripts, methods) |

Filename tags: `*.server.clpp` → Script, `*.client.clpp` → LocalScript, untagged → ModuleScript.

Access: `player.Name` (property), `player::FindFirstChild` (method), `DataService:Server` (table). Range-for: `for (T x : list)`. Concatenation: `a .: b`. Output: `post` / `warn` / `report`.

## Install

```bash
cargo install --path .
clpp install
```

`clpp install` copies the language pack into **VS Code** and **Cursor** (and Insiders, VSCodium, and Windsurf when those products are present). Restart the editor so `.clpp` / `.clp` / `.clh` files get the CL++ icon, syntax highlighting, and IntelliSense.

## CLI

The binary is `clpp`:

```bash
clpp compile <file.clpp> [-o out.luau] [--json]
clpp build [dir] [-o out]
clpp api compile [--file file.clpp]   # JSON stdin/stdout — Cluaupp contract
clpp api manifest
clpp install [--editor cursor|code]
clpp manifest
```

## Layout

```
CL++/
├── src/            Rust compiler (pest + AST + Luau codegen)
├── docs/           language specification
├── examples/       sample programs
├── stdlib/         IntelliSense headers
├── tests/golden/   reference Luau
├── editors/vscode  language pack (highlight, icon, IntelliSense)
└── support/        Cluaupp TypeScript contract
```

## Spec

- [Syntax](docs/spec/syntax.md)
- [Files](docs/spec/files.md)
- [Types](docs/spec/types.md)
- [Emit](docs/spec/emit.md)
- [Cluaupp support](docs/cluaupp-support.md)

## License

MIT
