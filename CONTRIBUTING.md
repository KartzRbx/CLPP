# Contributing

CL++ is the language (syntax, semantics, OOP). Roblox API wiring belongs in [Cluaupp](https://github.com/KartzRbx/Cluaupp).

## Build

```bash
cargo test
cargo run -- compile examples/hello/hello.server.clpp
```

Keep user-facing copy in English: CLI help, README, docs, editor pack, and example strings.

## Layout

| Path | Role |
| --- | --- |
| `src/parser/grammar.pest` | PEG grammar |
| `src/codegen/` | Luau emit |
| `docs/spec/` | language specification |
| `examples/` | sample programs |
| `tests/golden/` | expected Luau |

If you change emit, regenerate goldens by compiling the examples and updating `tests/golden/`.
