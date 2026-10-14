# Comments and file directives

A comment explains intent to a reader. It is not executed. CL++ uses `<<` for line comments and `<<[ ... ]>>` for block comments. A directive requests a compiler mode through `pragma strict;`.

## Line comments

```clp
<< Start with full health.
number hp = 100;
hp -= 20; << Damage from the first hit.
post(hp);
```

`<<` begins a comment that continues to the end of the physical line. A comment can occupy a whole line or follow code. Put `<<` at the start of each line when writing a longer note. Comment delimiters inside quoted text are text, not comments.

## Block comments

Use `<<[ ... ]>>` for a note across several lines. Close it with `]>>`. Blocks do not nest; the first closing delimiter ends the comment. An unclosed block comment is a lexical error.

```clp
<<[ Explain a longer rule
   across multiple lines. ]>>
number hp = 100;
```

## Strict checking

```clp
pragma strict;
number hp = 100;
post(hp);
```

`pragma` introduces a file-level directive; `strict` requests strict checking. The semicolon ends the directive. It does not create a variable or a function. The intended strict checks include type compatibility and exhaustive matching; [Diagnostics](../guide/11-errors.md) explains their status.

## Common writing mistakes

A newline ends a line comment. It does not replace the semicolon on the preceding instruction. A comment cannot close a missing brace or repair an unfinished string.

## Compiler status

These comment forms and directives are lexer/parser-tested. C++ comment delimiters are not CL++ syntax. Text such as `<<!strict` is an ordinary line comment and does not select a checking mode. `pragma strict;` or `pragma nostrict;` must occur once before declarations and links. Unknown modes, including `nocheck`, are rejected. See [Implementation status](status.md) for other language features.

Next: [Identifiers](identifiers.md), [Literals](literals.md), [Punctuation](punctuation.md).
