# 2. Lexical syntax

This chapter describes comments, names, keywords, and literals. See the [lexer reference](../reference/lexer.md) for the complete tokenization rules.

## Comments and directives

`<<` starts a line comment. `<<[ ... ]>>` encloses a block comment. Line endings may be LF, CRLF, or CR. `//` is not a comment marker.

```clp
<< This is a line comment.
post(1); << A comment can follow a statement.
<<[ A block comment can span
multiple lines. ]>>
```

At the start of a file, `<<!strict` enables strict literal and union type checking; `<<!nocheck` disables type checking for that file.

## Identifiers and keywords

Identifiers start with a letter or `_` and continue with letters, digits, or `_`. Names are case sensitive. Use `camelCase` or `snake_case` for variables and functions, and `PascalCase` for types and module aliases. Reserved names appear in the [keywords reference](../reference/keywords.md).

## Literals

Numbers include integers (`42`, `0xFF`, `0b1010`), decimals (`3.5`), exponents (`1e3`), and separators between digits (`10_000`). `true` and `false` are Boolean literals; `null` represents the absence of a value.

Strings use either single or double quotes and support `\n`, `\t`, `\\`, `\"`, and `\'`. A backtick string is a template: `${expression}` inserts the evaluated expression, and `\${` writes a literal `${`.

```clp
let name = "Ada";
let level = 3;
post(`${name} reached level ${level}`);
```

Every statement ends with `;`. Braces `{ ... }` delimit a block and create a new variable scope.

## Choosing a literal

Use an integer for counts, indices, and exact whole numbers; use a decimal for measurements and interpolation. Use a template when a message mixes several values, and a plain string when it is fixed. Avoid using a template for every concatenation: `.:` is clearer for a short two-part message. Comments should explain intent or constraints; code that merely restates itself gains little from a comment.
