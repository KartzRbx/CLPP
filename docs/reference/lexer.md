# Lexer

The lexer recognizes the current source spelling and reserves removed keywords to issue syntax errors. Reserved obsolete words are excluded from completion and highlighting. See [implementation status](status.md). Token names and internal APIs are implementation details, not additional language features.

## What a lexer does

The lexer separates source into tokens before parsing. In `number score = 10;`, the tokens are a type keyword, a name, `=`, an integer literal, and `;`. Parsing decides how those tokens form a declaration; type checking decides whether the value fits the declared type.

## Whitespace and positions

Spaces, tabs, and line breaks separate words. Indentation is for readability; braces delimit bodies. A line break does not replace a statement semicolon. Diagnostics should identify the source file, line, and column so a developer can locate the offending token.

## Names and keywords

Identifiers begin with a letter or `_`, followed by letters, digits, or `_`. Use ASCII names for portability. Names are case sensitive: `score` and `Score` differ. Keywords such as `const`, `def`, and `return` have special meaning and cannot serve as ordinary variable names. [Identifiers](identifiers.md) explains names separately; [Keywords](keywords.md) groups the vocabulary by topic.

## Comments and checking mode

| Source | Meaning |
| --- | --- |
| `<< text` | Comment through the end of the line |
| `<<[ text ]>>` | Block comment; may span lines |
| `pragma strict;` | File checking directive, compiler-tested |

Comments do not execute. Use a separate `<<` on each line of a longer note. Documentation-comment syntax and additional checking modes are not yet specified here. Read [Comments](comments.md) for examples.

## Numeric literals

| Source | Value |
| --- | --- |
| `42` | Decimal integer |
| `0xFF` | Hexadecimal integer |
| `0b1010` | Binary integer |
| `3.5` | Floating-point value |
| `1e3` | Scientific notation |
| `10_000` | Integer with digit separators |

A separator belongs between digits in the same numeric component. A base prefix requires digits and an exponent requires an exponent value. The sign in `-42` is an operator before the number. [Literals](literals.md) explains when to choose each form.

## Strings and templates

Single and double quotes delimit text strings. Escape sequences include `\n`, `\t`, `\\`, `\"`, and `\'`. Backtick templates insert a value through `${expression}`; `\${` writes the interpolation marker literally. Literal decoding and interpolation happen after tokenization.

## Symbols

The lexer must distinguish multi-character symbols from their shorter prefixes: `<=` from `<`, `+=` from `+`, and `...` from `..` and `.`. Parentheses, brackets, braces, commas, colons, and semicolons are separate punctuation tokens.

- `.` selects a member; `::` selects a scoped case such as `Element::Fire` or an established native member.
- `.:` concatenates text; `..` denotes a range or slice boundary.
- `=>` introduces a target match arm. Anonymous callbacks use `func (...) -> Type { ... }`.
- `~>` is part of the connection/janitor design; its final lifecycle contract still needs reconciliation with the old signal runtime.
- `@` begins a standard-library module path after `link`; annotations and receiver shortcuts require separate contracts.
- `and`, `or`, `not`, `shl`, and `shr` are word operators.

The parser determines the meaning of a symbol in context. See [Punctuation](punctuation.md) and [Operators](../guide/04-operators.md).

## Errors belong to different stages

Excessive recursive syntax is rejected with `syntax nesting limit exceeded`
instead of overflowing the compiler's native stack. Expressions, unary chains,
statement nesting and type annotations each have a budget of 80 active parser
entries. Ordinary code, including 64 nested grouping parentheses, is tested.
This is a compiler resource limit, not a replacement for runtime recursion rules.
Iterative operator, call, index and member chains also have a maximum expression
tree depth of 128 edges. This catches flat source that would otherwise build an
unsafe recursive tree; a chain of 128 numeric terms remains tested and supported.

Malformed digits, unfinished strings, unfinished comments, and unknown characters are lexical errors. Missing punctuation is a parser error. Assigning an incompatible value is a type error. A runtime failure occurs after compilation, for example when executing an invalid operation. Read [Errors](../guide/11-errors.md) to understand those stages.

Implementation token families currently include integer and floating-point literals rather than one generic number token. Template tokenization may split interpolated text into head, middle, and tail segments. These internal details do not change the source spelling taught here.
