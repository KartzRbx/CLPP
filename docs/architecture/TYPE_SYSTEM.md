---
title: Type system (what is specified today)
description: Formal surface of the TypeId checker — implemented rules vs intentional gaps.
---

# Type system (what is specified today)

CL++ checks types with interned `TypeId`s (`src/types`, `src/checker/typed`). Grammar alone is not the type system ([FEATURE_CHECKLIST](FEATURE_CHECKLIST)).

## Implemented surface

| Construct | Syntax | Notes |
| --- | --- | --- |
| Primitives | `int` `float` `bool` `string` `void` | Emit Luau `number` / `boolean` / `string` |
| Nominal | `Player`, user `struct` / `interface` | Members + inheritance IS-A |
| Optional | `optional<T>` / `T?` | = `T \| null`; narrowed by `guard` / truthy `if` |
| Union / intersection | `A \| B`, `A & B` | `type` / `using` aliases |
| Containers | `array<T>` `dictionary<K,V>` `signal<…>` | |
| Aliases | `type Name = …;` `using Name = …;` | Including `type Box<T> = array<T>` |
| Enums | `enum` / `enum class` | Exhaustiveness on `match`/`switch` (CLPP0501) |
| Checked generics | `template <typename T : Bound>` | Body + call-site bounds ([RFC 0010](https://github.com/KartzRbx/CLPP/blob/main/rfc/0010-checked-generics.md)) |
| Comptime reflection | `comptime { }` + builtins | ([RFC 0009](https://github.com/KartzRbx/CLPP/blob/main/rfc/0009-comptime-reflection.md)) |
| Assignability | `is_subtype` / optional→plain ban | CLPP0201 |
| Casts | `static_cast` / `as` | Emit value; no runtime ClassName check |

## Function types (honest)

| Form | Status |
| --- | --- |
| `func` | Emits `(...any) -> any` — **loses** parameter/return detail |
| Grammar `function<Ret(Args)>` | Parsed; full checking still incomplete |
| Goal | `(Player, int) -> bool` as a first-class `TypeKind::Function` everywhere |

Prefer explicit parameter types on named functions; treat bare `func` as a temporary escape hatch.

## Inference (`auto`)

Inferred from `new Class(...)`, `GetService<T>()`, datatype constructors, and some locals. Parameters and struct fields should stay explicit.

## Conversions

| Kind | Rule |
| --- | --- |
| `int` → `float` | Allowed (widening) |
| `float` → `int` | Warning / reject in typed assign paths |
| `optional<T>` → `T` | Error unless narrowed |
| `static_cast` / `as` | Explicit; no runtime check |
| `to_string` / `to_number` / `to_bool` | Builtins |

## Gaps called out on purpose

Not missing by accident — tracked under [RFC 0008](https://github.com/KartzRbx/CLPP/blob/main/rfc/0008-intent-system-roadmap.md) / Intent:

- Variance / full function subtyping
- `import type` / star imports
- Richer patterns (literal / nested / guarded)
- Overload resolution (currently **forbidden**)
- Distinct/nominal newtypes beyond aliases
- Effects / contracts / typestate (Phase A+)

## OOP model (explicit)

CL++ OOP is **`struct` / `class` / `interface` + methods + optional parent** emitting Luau tables — not full C++ (no virtual table language feature, no destructor language feature). `public` / `private`, `@this`, and inheritance members are checked; traits/abstract/`final` are not a separate surface yet.
