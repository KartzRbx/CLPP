# 2. Lexical syntax

The examples on this page run in the documentation checks. See [implementation status](../reference/status.md) for the remaining language work.

The lexer reads your source as **tokens**: words, values, and symbols. The parser then combines those tokens into declarations, expressions, and statements. For example, `number score = 10;` contains a type, a name, an assignment symbol, a number, and a statement terminator.

## Names and whitespace

Names begin with a letter or `_`, followed by letters, digits, or `_`. Use ASCII names for portability. `score`, `player2`, and `_count` are valid; `2player` is not. Names are case sensitive: `score` and `Score` are different. A keyword such as `const` cannot be a variable name.

Spaces, tabs, and line breaks separate tokens. Indentation is for people; braces determine the body of a block. Write one simple statement per line while learning.

## Comments and checking mode

`<<` starts a comment through the end of the line. Comments explain intent and do not execute. For a longer note, begin each line with `<<` or use a `<<[ ... ]>>` block comment. The first `]>>` closes a block; blocks do not nest.

```clp
<< A comment before a statement.
post(1); << A comment after a statement.
<< A longer note can continue
<< on another commented line.
```

Use `pragma strict;` at the beginning of a file to request strict checking, or `pragma nostrict;` for the default gradual mode. A directive must precede imports and declarations, and may appear only once. Unknown modes are rejected. See [comments and directives](../reference/comments.md).

## Numbers and Boolean values

| Source | Meaning |
| --- | --- |
| `42` | Decimal whole number |
| `0xFF` | Hexadecimal whole number |
| `0b1010` | Binary whole number |
| `3.5` | Fractional number |
| `1e3` | Scientific notation: 1000 |
| `10_000` | Digit separators: 10000 |
| `true`, `false` | Boolean values |
| `null` | Absence of a value |

A separator belongs between digits: `10_000` is valid, while `10_`, `1__0`, and `0x_FF` are invalid. The minus sign is an operator before a number, rather than part of its numeric token.

## Strings and templates

Single and double quotes both create strings. Escape sequences include `\n` (newline), `\t` (tab), `\\` (backslash), `\"`, and `\'`. A normal quoted string must close on the same line. A backtick template inserts an expression with `${expression}`; `\${` writes that marker literally.

```clp
const string name = "Ada";
number level = 3;
post(`${name} reached level ${level}`);
post("first line\nsecond line");
```

```output
Ada reached level 3
first line
second line
```

## Punctuation and scope

`;` ends a simple statement. `(...)` groups an expression or encloses arguments and conditions. `,` separates arguments. `{...}` encloses a block. `[...]` indexes a value. `.` selects a member. A name declared in a block belongs to that block; code outside cannot read it.

```clp
number total = 2;
if (total > 0) {
  string message = "positive";
  post(message);
}
post(total);
```

Read the dedicated references for [identifiers](../reference/identifiers.md), [literals](../reference/literals.md), [punctuation](../reference/punctuation.md), and the [lexer implementation](../reference/lexer.md).
