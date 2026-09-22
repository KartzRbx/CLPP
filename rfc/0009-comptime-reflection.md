# RFC 0009 — Comptime + reflection (Intent Phase A #1)

Status: Accepted for implementation (subset)

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Problem

Intent Phase A starts with **compile-time inspection** of nominal types so serializers, validators, and docs can be generated without a Luau reflection API.

## Syntax

```text
comptime {
  static_assert(field_count(PlayerData) == 3);
  // later: generate / expand (not in this RFC subset)
}
```

- `comptime { … }` is a statement (`Stmt::Comptime`).
- Body is type-checked with `comptime = true`.
- **Emit:** the block is erased from Luau (zero runtime cost).

## Reflection builtins (comptime-only)

| Builtin | Args | Meaning |
| --- | --- | --- |
| `type_name(T)` | type name | Nominal label |
| `field_count(T)` | type name | Number of fields on struct `T` |
| `field_names(T)` | type name | Field names (compile-time list) |
| `has_field(T, "name")` | type + string | Whether `T` has that field |

Using these **outside** `comptime { }` is a hard error.

`static_assert` remains available both inside and outside comptime (CLPP0701).

## Typing rules (subset)

1. Argument to reflect builtins must name a known struct/enum (or string literal of that name).
2. Unknown type → diagnostic from typed checker.
3. Comptime body does not contribute to function return-path analysis (erased).

## Non-goals (this RFC)

- Code generation / quasiquotes
- Full const-eval of arbitrary expressions
- Effects, contracts, typestate (later Phase A RFCs)
- Runtime reflection in Luau

## Implementation map (FEATURE_CHECKLIST)

| Layer | Location |
| --- | --- |
| Grammar | `KW_COMPTIME`, `comptime_stmt` |
| AST | `Stmt::Comptime { body, span }` |
| Binder | walks body in a block scope |
| Checker | `checker::typed` + `intent::is_reflect_builtin` |
| Codegen | erase |
| Tests | `tests/v07.rs` comptime cases |

## Success

- `cargo test` green
- Comptime-only builtins rejected at runtime site
- Private / continue / static_assert owned by typed (no dual-pass duplicates)
