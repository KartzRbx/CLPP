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

## Optional conditions and omitted fields

An omitted `optional<T>` field defaults to `null` unless the field declares an explicit default. Required fields still need a value or a declared default. This also applies to imported definitions, aliases, inherited fields and generic specializations.

For optional values, conditions and `!` test **null and boolean false**. Zero, an empty string and an empty collection are present values. `if`, `guard`, loops, ternary conditions and the operands of `and`/`or` use this same rule. Short-circuit operators evaluate only the operands they need; each operand is evaluated once. Existing non-optional numeric conditions retain their numeric behavior.

```clp
def Human {
  string name;
  optional<number> cpf;
}
bool CreateBankAccount(Human target) {
  guard (target.cpf) else { return false; }
  return true;
}
post(CreateBankAccount(Human { name = "Kartz" }));
post(CreateBankAccount(Human { name = "Kartz", cpf = 0 }));
optional<bool> enabled = false;
post(!enabled);
optional<number> zero = 0;
post(!zero);
```

```output
false
true
true
false
```

Use `value != null` when `false` is a valid value you want to keep. A successful optional condition narrows a local variable to its payload type; the failure branch can still contain either `null` or `false`. Assigning to a refined variable invalidates its refinement. Repeated object field reads are not automatically refined across mutations: store the field in a local variable before using its payload.

## Runtime type tests

Use `value:is<Type>` without parentheses after the type. Completion after `value:` or `value:i` offers the `is<Type>` method with the type selected for editing. The result is a boolean and the value is evaluated once. Combine conditions with `and`:

```clp
def Human { string name; optional<number> cpf; }
any kartz = Human { name = "Kartz" };
if (kartz:is<Human> and kartz.cpf == null) {
  post("missing cpf");
}
post(kartz:is<number>);
```

```output
missing cpf
false
```

A successful test narrows a local variable in the matching branch, in the right operand of `and`, or after an exiting `guard`. Negating a test also allows refinement in the corresponding `else`/`or` branch. Assignments invalidate refinement. Imported types such as `H.Human`, aliases and nominal inheritance are supported: a child is also its base, but an unrelated definition with identical fields is not the same type.

Runtime checks distinguish `number` (integer), `float`, `bool`, `string`, vectors, buffers, connections and nominal non-generic definitions. `any`, optional versions and unions of supported types are accepted. Erased types, including enum identity, literal types, generic specializations, signal signatures and typed collections, produce a diagnostic rather than an unsound refinement. Native values whose producers do not attach a nominal tag cannot be identified as a data definition through `any`. This syntax does not perform conversion; use an explicit cast for conversion.

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
