# 4. Operators

CL++ supports arithmetic `+ - * / %`, compound assignment `+= -= *= /= %=`, and postfix `++ --`. Integer division truncates toward zero; division by zero raises a catchable runtime error.

Comparisons `== != < <= > >=` return `bool`. The logical operators `and`/`&&`, `or`/`||`, and `not`/`!` short circuit. Bitwise operators are `&`, `|`, `^`, `~`, `shl`, and `shr`; `^` means XOR, not exponentiation. For powers use `Axiom.Pow`.

`.:` concatenates values as text, and `condition ? yes : no` selects one expression. `value[index]` indexes a collection or string; `value[start .. end]` returns a slice excluding `end`. Vector addition acts componentwise, and multiplication by a number scales a vector.

```clp
let hp = 90;
post("HP: " .: hp);
post(hp > 0 ? "alive" : "defeated");
post("abcdef"[1 .. 4]);
post(Vector3(0, 10, 0) + Vector3(2, 0, 1) * 0.5);
```

Use parentheses when combining operators whose precedence is unclear. In particular, write `"total: " .: (a + b)` for concatenation after addition and `(1 shl 3) == 8` for a shift before comparison. A struct can define its own addition through `func operator+`.

The [operators reference](../reference/keywords.md) lists every operator and spelling.

## Practical choices

Use `and` or `or` when a second condition should only run if needed, such as checking an index before reading a list. Use `&`, `|`, and shifts for integer flags and binary protocols, not for ordinary Boolean decisions. Use `.:` or a template for presentation text; keep numeric computation numeric until the final output step. A ternary is useful for two short values; use `if`/`else` when either branch has several statements. Add parentheses whenever precedence would make a reader stop to calculate the order.
