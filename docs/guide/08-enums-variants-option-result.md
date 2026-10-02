# 8. Enums, variants, Option, and Result

An `enum` names a fixed set of cases. A case can carry a value, which a `match` arm can unpack. A `variant` stores a value of one of several listed types.

```clp
enum State {
  Idle,
  Moving(int)
}

func describe(State state) {
  match (state) {
    Idle ~> post("idle");
    Moving(speed) ~> post(speed);
  }
}
describe(State.Moving(3));
```

`Option<T>` represents a value that may be absent. `Result<T, E>` represents success or an error value. Match their cases to handle both possibilities explicitly instead of assuming a value exists.

`type Name = A | B;` defines a union type; `A & B` is an intersection. With `<<!strict`, literal and union types are checked more rigorously. Use these tools when a field or result can legitimately take several forms. See [control flow](05-control-flow.md) for match guards and wildcard patterns.

## Choose the right representation

| Situation | Prefer | Reason |
| --- | --- | --- |
| A fixed set of named states | `enum` | Callers can see and match every case |
| One value may have several unrelated types | `variant` | The possible stored types are explicit |
| A value may be absent | `Option<T>` | The missing case is handled deliberately |
| An operation may fail with information | `Result<T, E>` | Success and failure both have values |
| A constrained set of alternative types | Union type | The type checker can narrow the possibilities |

Avoid a sentinel such as `-1` when it could also be a valid value; `Option<T>` makes absence unambiguous. Avoid catching a runtime error for an expected domain outcome if a `Result` value can describe it directly.
