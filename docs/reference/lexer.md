# Lexer

The lexer in `src/core/lexer/lexer.cpp` turns source into tokens and diagnostics. `Lexer(const std::string&)` borrows its source string, so the source must outlive the resulting token views. Errors produce an `Invalid` token and a diagnostic while parsing continues.

## Line endings and comments

LF (`\n`), CRLF (`\r\n`), and CR (`\r`) terminate lines and line comments. `<<` starts a line comment, `<<[ ... ]>>` a block comment, `<<[[]] ... >>` a documentation comment, and `<<!strict`/`<<!nocheck` a file directive. `//` produces two `/` operator tokens.

## Literal tokens

| Source | Token |
| --- | --- |
| `42`, `3.5`, `1e3`, `0xFF`, `0b1010` | `Number` |
| `"..."`, `'...'` | `StringLiteral` |
| Backtick text with `${expr}` | `TemplateString` |

Underscores separate digits only within the same numeric component: `10_000` and `0xFF_00_FF` are accepted, while `10_`, `1__0`, and `0x_FF` produce `invalid numeric separator`. Decimal parsing is locale-independent.

## Operators and keywords

The lexer recognizes arithmetic, assignment, comparison, bitwise, punctuation, `.:`, `..`, `...`, `::`, `~>`, `=>`, and `@`. Word operators are `and`, `or`, `not`, `shl`, and `shr`. Keywords are looked up in a sorted table and exported to the editor through `keyword_names()`. See [Keywords and operators](keywords.md).

`@` is always a separate token. After `link`, it begins a standard library module path; inside a method, `@hp` means `self.hp`; elsewhere, it addresses the shared program table.

## Diagnostics

The lexer reports unterminated strings, templates, block or documentation comments; misplaced numeric separators; incomplete hexadecimal, binary, or exponent literals; and unexpected characters. A string interrupted by a newline does not consume the rest of the source file.
