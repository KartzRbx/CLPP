# 8. Enums, optional values, and unions

Alternative values should make the possible states clear. CL++ uses scoped enum cases, optional values for absence, and aliases for unions. These are distinct concepts with separate syntax.

Scoped enums, exhaustive matching, optional null checks, exiting guards and literal unions are compiler-checked. The examples below execute in the documentation suite and check their displayed results. See [implementation status](../reference/status.md) for broader type-system work.

## Scoped enums

```clp
enum class Direction { Up, Down, Left, Right }
Direction facing = Direction::Left;
post(facing == Direction::Left);
```

```output
true
```

`enum class` introduces scoped cases. `Direction::Left` selects a static enum member. Use `.` for an object's instance members and `::` for static members.

## Matching cases

```clp
enum class Direction { Up, Down, Left, Right }
Direction facing = Direction::Left;
match (facing) {
  Direction::Up => post("up");
  Direction::Down => post("down");
  Direction::Left => post("left");
  Direction::Right => post("right");
}
```

```output
left
```

`=>` separates the pattern from its action. Handle each possible case. See [control flow](05-control-flow.md) for guards and exhaustive handling. The old match-arm token is not the canonical syntax.

## Optional values and null

```clp
optional<number> score = null;
if (score != null) {
  post(score);
} else {
  post("No score yet");
}
```

```output
No score yet
```

`optional<T>` allows a `T` value or `null`. A check against null narrows the value to `T` on the branch where it exists.

## Guarding an absent value

```clp
void PrintScore(optional<number> score) {
  guard (score != null) else { return; }
  post(score);
}
PrintScore(null);
PrintScore(42);
```

```output
42
```

The failing branch exits. After the guard, the checker knows the value is present. Here the absent score prints nothing, while the present score prints 42.

## Unions and literal unions

```clp
def Tile = "." | "o" | "#" | " ";
Tile floor = ".";
post(floor);
```

```output
.
```

A union accepts one of its alternatives. A literal union accepts exactly the listed values, which is useful for map tiles and constrained strings. `def Name = Type;` names the expression without creating a distinct object definition.

## Errors and data alternatives

Use `optional<T>` and `null` for absence, `def Name = A | B;` for a union, and [exceptions](11-errors.md) for recoverable failures. Scoped enums contain named cases without payload syntax; combine a scoped enum with explicit data fields when a case carries data. The compiler rejects legacy payload enums, `variant`, `Some`/`None`, and the built-in `Ok`/`Err` wrappers. Ordinary user declarations may use these identifiers; they have no intrinsic meaning.
