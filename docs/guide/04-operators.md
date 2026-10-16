# 4. Operators

This page describes current CL++ forms. Executable examples are checked by the documentation suite; explicitly target-marked proposals remain separate. See [implementation status](../reference/status.md).

CL++ supports arithmetic `+ - * / %`, compound assignment `+= -= *= /= %=`, and postfix `++ --`. Integer division truncates toward zero; division by zero raises a catchable runtime error.

Comparisons `== != < <= > >=` return `bool`. `and`/`&&` and `or`/`||` short circuit: the right operand runs only when needed. `not`/`!` negates one condition. Bitwise operators are `&`, `|`, `^`, `~`, `shl`, and `shr`; `^` means XOR, not exponentiation. For powers use `Axiom.Pow`.

`.:` concatenates values as text, and `condition ? yes : no` selects one expression. `value[index]` indexes a collection or string; `value[start .. end]` returns a slice excluding `end`. Vector addition acts componentwise, and multiplication by a number scales a vector.

```clp
number hp = 90;
post("HP: " .: hp);
post(hp > 0 ? "alive" : "defeated");
post("abcdef"[1 .. 4]);
post(Vector3(0, 10, 0) + Vector3(2, 0, 1) * 0.5);
```

Use parentheses when combining operators whose precedence is unclear. In particular, write `"total: " .: (a + b)` for concatenation after addition and `(1 shl 3) == 8` for a shift before comparison. The restricted data operator forms `operator+` and `operator-` exist; they do not imply arbitrary operator support. See [less common forms](../reference/language-contract.md#12-less-common-forms-and-limits).

The [operators reference](../reference/keywords.md) lists every operator and spelling.

## Practical choices

Use `and` or `or` when a second condition should only run if needed, such as checking an index before reading a list. Use `&`, `|`, and shifts for integer flags and binary protocols, not for ordinary Boolean decisions. Use `.:` or a template for presentation text; keep numeric computation numeric until the final output step. A ternary is useful for two short values; use `if`/`else` when either branch has several statements. Add parentheses whenever precedence would make a reader stop to calculate the order.

## Reading an expression

An expression produces a value: `2 + 3` produces `5`. `=` assigns a value, while `==` compares two values. `hp += 5;` is a statement updating a variable. `hp == 5` is a Boolean expression usable in a condition.

```clp
number hp = 5;
hp += 2;
post(hp == 7);
post(not (hp <= 0));
post(2 + 3 * 4);
post((2 + 3) * 4);
```

```output
true
true
14
20
```

## Grouping expressions

Use parentheses to make evaluation order explicit: `(2 + 3) * 4`, `not (hp <= 0)`, and `(1 shl 3) == 8`. Arithmetic, comparison, and logical operators should each be readable as separate steps. Prefix operators bind before arithmetic. Comparisons share one precedence level and bind before shifts in the current parser. See the exact [precedence table](../reference/language-contract.md#3-expressions-access-and-precedence); do not assume another language's grouping.

Assignment is a statement: `score = 10;`. Equality is an expression: `score == 10`. Avoid chained comparisons; write `score >= 0 and score <= 100`.
