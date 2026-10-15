# 5. Control flow

This page describes current CL++ forms. Executable examples are checked by the documentation suite; explicitly target-marked proposals remain separate. See [implementation status](../reference/status.md).

Control flow chooses which statements run and how often. A condition uses `(...)`; an `if`, `while`, or `for` body always uses `{...}`, even for one statement. `true`, nonzero numbers, and nonempty strings are truthy; `false`, zero, and empty strings are falsey. Prefer explicit comparisons in beginner code.

## `if` and `else`: choose a branch

The first true condition runs. An optional `else` handles the remaining case.

```clp
number hp = 35;
if (hp <= 0) {
  post("defeated");
} else if (hp < 50) {
  post("wounded");
} else {
  post("healthy");
}
```

```output
wounded
```

Declare a value before the condition when it is needed there. A variable created inside a branch stays in that branch's scope.

## `while`: repeat while a condition is true

The condition is checked before every iteration. If it starts false, the body never runs. Change the state or provide a clear exit so the loop can finish.

```clp
number count = 0;
while (count < 3) {
  post(count);
  count++;
}
```

```output
0
1
2
```

## `for`: initializer, condition, step

The initializer runs once, the condition is checked before each iteration, and the step runs after the body. Declare the counter in the initializer. Its type, starting value, condition, and update are visible together.

```clp
for (number i = 0; i < 3; i++) {
  post(i);
}
```

```output
0
1
2
```

## Typed collection iteration

`for (Type item in collection)` visits each item, giving it a name and a type inside the loop body. Choose this form when you need values rather than indices.

```clp
array<string> names = array<string>("Ada", "Lin");
for (string name in names) {
  post(name);
}
```

```output
Ada
Lin
```

For explicit index control, use a typed counter and `while (index < len(names))`, reading `names[index]` and advancing the counter on every iteration. Indices start at zero.

## `break` and `continue`

`break;` exits the nearest loop. `continue;` skips the rest of its body and starts the next iteration. In a `for`, the step still runs after `continue`. In a `while`, advance a manual counter before any `continue` that would otherwise skip it.

```clp
for (number i = 0; i < 6; i++) {
  if (i == 1) { continue; }
  if (i == 4) { break; }
  post(i);
}
```

```output
0
2
3
```

## `switch`: choose by value

`case value:` selects a matching case. `default:` handles other values. Cases do not fall through, so a case does not require a trailing `break`.

```clp
number direction = 1;
switch (direction) {
  case 0: post("left");
  case 1: post("right");
  default: post("stay");
}
```

```output
right
```

## `match`: patterns and guards

An arm has a pattern, optional `guard condition`, `=>`, and a statement or block. The first matching arm runs. `_` accepts any remaining value. Payload patterns are not part of enum syntax; use an explicit data definition and match its scoped enum discriminator. See [enums and optional values](08-enums-variants-option-result.md).

```clp
number score = 85;
match (score) {
  100 => post("perfect");
  _ guard score >= 80 => post("great");
  _ => post("keep trying");
}
```

```output
great
```

Use `if` for general conditions, `switch` for alternatives based on one value, `match` for patterns and guards, and `while` for changing state. `return` leaves a function; [functions](06-functions.md) explains return types and values. [Errors](11-errors.md) covers `try` and `catch` for failures rather than ordinary branching.

## Testing after the body

```clp
number attempts = 0;
do {
  attempts += 1;
  if (attempts < 3) { continue; }
  post(attempts);
} while (attempts < 3);
```

```output
3
```

A do/while body runs at least once. Continue tests its closing condition; break exits without testing it. The condition cannot reference locals declared inside the body's block.
