# Literals

A literal writes a value directly in source code. `42` is a number; `"Ada"` is text. A type name describes which values are allowed, while a literal supplies one value.

## Numbers

| Form | Example | Meaning |
| --- | --- | --- |
| Decimal integer | `42` | Whole number |
| Hexadecimal integer | `0xFF` | Base 16, value 255 |
| Binary integer | `0b1010` | Base 2, value 10 |
| Decimal fraction | `3.5` | Floating-point value |
| Scientific notation | `1e3`, `2.5e-3` | Decimal exponent |
| Digit separators | `10_000`, `0xFF_00` | Underscores aid reading |

```clp
number points = 10_000;
float speed = 2.5;
post(points);
post(speed);
post(0xFF);
```

Separators go between digits within the same numeric component. `10_`, `1__0`, and `0x_FF` are invalid. A minus sign is an operator applied to a number, not part of the unsigned literal token. Integer division and floating-point division have different results; see [Operators](../guide/04-operators.md).

Integers must fit signed 64-bit `number`. The negative boundary `-9223372036854775808` is valid; a positive integer above `9223372036854775807` is rejected. Decimal literals use `float`.

## Strings

Both single and double quotes delimit strings. Escape sequences represent characters that are awkward to type directly.

| Escape | Value |
| --- | --- |
| `\n` | Newline |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |
| `\'` | Single quote |

```clp
string name = "Ada";
post('Hello, ' .: name);
post("first\nsecond");
```

Quotes must close. Write a newline as `\n` inside a string rather than interrupting a quoted string with a physical newline.

## Template strings

Backticks delimit a template. `${expression}` evaluates an expression and inserts its display value.

```clp
string name = "Ada";
number hp = 75;
post(`${name}: ${hp} HP`);
```

Use `\${` for a literal interpolation marker. Templates differ from plain strings: `"${hp}"` does not interpolate. See [Collections and text](../guide/09-collections-and-text.md) for indexing and slicing.

## Booleans and absence

`true` and `false` are Boolean values. `null` represents an absent value in the target optional system.

```clp
optional<number> score=null;
if(score!=null) { post(score); } else { post("absent"); }
score=0; if(score!=null) { post(score); }
```

```output
absent
0
```

An optional must be checked or narrowed before using it as its contained type. `0`, `false`, and an empty string are values, not spelling alternatives for `null`. See [Optional values](../guide/08-enums-variants-option-result.md).

## Aggregate initialization

```clp
def Vec2 { number X = 0; number Y = 0; }
Vec2 position = Vec2 { X = 3, Y = 4 };
array<number> scores = { 10, 20, 30 };
post(position.X);
post(scores[1]);
```

```output
3
20
```

Braces initialize a data value or an array; named fields use `Field = value`. Omitted fields follow defaults defined in the type. An array initializer gets its element type from its declaration, return type, field, assignment or function parameter; aliases and nested arrays retain that type, and elements are checked individually. Without an array type context, use an explicit constructor such as `array<number>(10, 20, 30)`. See [Data definitions](../guide/07-structs-and-inheritance.md).
