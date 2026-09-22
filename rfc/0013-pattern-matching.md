# RFC 0013 — Rich pattern matching (Intent Phase A)

Status: Accepted — subset shipping (Option/Result tags + existing instance `match`)

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface

```clpp
match (r) {
  Ok v => { return v; },
  Err e => { return fallback; },
}
match (opt) {
  Some x => { use(x); },
  None => { },
}
```

## Rules

- Discriminant typed as `Result<T,E>` requires `Ok` + `Err` arms or `_`.
- Discriminant typed as `Option<T>` / `optional<T>` requires `Some` + `None` or `_`.
- Diagnostic: `CLPP1102`.
- Emit: tag tests on `{ ok = … }` / `{ err = … }` / nil for Option.

## Later

Nested patterns, or-patterns, guards, enum ADTs beyond Option/Result.
