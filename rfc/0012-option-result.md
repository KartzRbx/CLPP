# RFC 0012 — Option / Result as core Intent types

Status: Accepted — subset shipping

Parent: [RFC 0008](0008-intent-system-roadmap.md) Phase A

## Problem

`optional<T>` already exists as `T | null`. Intent Phase A also wants an explicit **Option** spelling and a **Result** for recoverable errors — without C++ exceptions or a second runtime.

## Surface

| CL++ | Meaning | Luau emit |
| --- | --- | --- |
| `Option<T>` | alias of `optional<T>` | same as optional |
| `None` | absent optional | `nil` |
| `Some(x)` | present optional | `x` |
| `Result<T, E>` | ok / err tagged union | nominal type + table tags |
| `Ok(v)` | success | `{ ok = v }` |
| `Err(e)` | failure | `{ err = e }` |

## Typing (subset)

- `Option<T>` ≡ `optional<T>`
- `Some(x)` : `Option<T>` when `x : T`
- `None` : `Option<T>` (nil)
- `Ok(v)` : `Result<T, E>` when `v : T` (E unconstrained at construction)
- `Err(e)` : `Result<T, E>` when `e : E`
- Assigning `Result` → plain `T` is a type error (same spirit as optional → plain)

## Explicitly later

- Nested patterns / ADTs beyond Option/Result tags (see RFC 0013 shipping subset)
- Effect integration (`throws E`)

## Shipping

- Exhaustive `match` on Result/Option tags (`CLPP1102`)
- `?` operator / try sugar (`CLPP1101`)

## Tests

`tests/option_result.rs` — assign / `?` / exhaustive match. Smoke also in `tests/v07.rs`.
