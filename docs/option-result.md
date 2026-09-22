# Option and Result (RFC 0012 / 0013)

CL++ ships Intent core types for absence and recoverable errors.

## Option

| CL++ | Luau |
| --- | --- |
| `Option<T>` / `optional<T>` | `T?` |
| `Some(x)` | `x` |
| `None` | `nil` |

```clpp
optional<int> maybe = Some(1);
match (maybe) {
  Some v => { post(v); },
  None => { },
}
```

## Result

| CL++ | Luau |
| --- | --- |
| `Result<T, E>` | tagged table |
| `Ok(v)` | `{ ok = v }` |
| `Err(e)` | `{ err = e }` |
| `expr?` | early-return on `err` |

```clpp
Result<int, string> parse(string s) {
  if (s == "") { return Err("empty"); }
  return Ok(to_number(s));
}

int use(string s) {
  int n = parse(s)?;
  return n + 1;
}

int unwrap_or(Result<int, string> r, int fallback) {
  match (r) {
    Ok v => { return v; },
    Err e => { return fallback; },
  }
}
```

Assigning a `Result` to a plain `T` is `CLPP0202`. Non-exhaustive Option/Result `match` is `CLPP1102`. `?` on a non-Result is `CLPP1101`.
