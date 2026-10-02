# 5. Control flow

Every `if`, `while`, and `for` body uses braces, including single-statement bodies. A condition is true for `true`, a nonzero number, or a nonempty string.

```clp
let hp = 35;
if (hp <= 0) {
  post("defeated");
} else if (hp < 50) {
  post("wounded");
} else {
  post("healthy");
}
```

`if (let value = expression)` declares a value scoped to the conditional. `while (condition)` repeats while its condition is true.

## Loops

`for (let item in source)` iterates a number (`0` through `n - 1`), a list or array (elements), a string (characters), or a dictionary (keys). The C-style `for (initializer; condition; step)` gives explicit control of the counter. `break` exits a loop and `continue` advances to the next iteration.

```clp
for (let i in 3) { post(i); }
for (let mut i = 10; i > 0; i -= 4) { post(i); }
```

## `switch` and `match`

`switch` compares a value with `case` expressions; `default` handles everything else. Cases do not fall through. `match` executes the first matching arm, written `pattern ~> statement;`. A literal matches itself, `_` matches anything, and `guard condition` adds a condition. Match arms can also unpack enum cases with data.

```clp
let score = 85;
match (score) {
  100 ~> post("perfect");
  _ guard score >= 80 ~> post("great");
  _ ~> post("keep trying");
}
```

## Which construct fits?

Use `if` for a few conditions, `switch` for simple alternatives based on one value, and `match` when cases carry data or need guards. Use `for ... in` to visit a whole collection or a fixed count; use the C-style `for` when the start, stop, and step are all important. `while` fits a loop controlled by changing state, such as a window frame loop. Avoid an unbounded `while` without a clear exit condition. Keep `break` and `continue` local and obvious; several nested early exits can make a loop difficult to follow.
