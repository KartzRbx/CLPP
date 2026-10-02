# Keywords and operators

This is a quick reference. For explanations and examples, start with [Lexical syntax](../guide/02-lexical-syntax.md) and [Operators](../guide/04-operators.md).

## Comments

`<<` starts a line comment; `<<[ ... ]>>` encloses a block comment. `<<!strict` and `<<!nocheck` at the start of a file are type-checking directives. `//` is not a comment marker.

## Reserved words

`Vector2` `Vector3` `Vector4` `abstract` `and` `as` `async` `atomic` `auto` `await` `bool` `break` `buffer` `case` `catch` `const` `constexpr` `continue` `cout` `decltype` `default` `double` `else` `endl` `enum` `extern` `false` `final` `float` `for` `from` `func` `guard` `if` `import` `in` `int` `join` `let` `link` `match` `move` `mut` `namespace` `new` `not` `null` `observable` `operator` `or` `override` `parallel` `pcall` `post` `private` `public` `report` `return` `shl` `shr` `signal` `spawn` `static` `string` `struct` `switch` `task` `thread` `true` `try` `type` `using` `variant` `void` `warn` `where` `while`.

The lexer supplies the editor's keyword list. `self` is the implicit first parameter of a method, not a reserved word.

## Built-in functions

`len` `list` `push` `pop` `insert` `remove` `find` `sort` `args` `mutex` `lock` `unlock` `fetch_add` `atomic_load` `actor`.

## Operators

| Category | Operators |
| --- | --- |
| Arithmetic | `+` `-` `*` `/` `%` |
| Assignment | `=` `+=` `-=` `*=` `/=` `%=` `++` `--` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logic | `and` / `&&`, `or` / `\|\|`, `not` / `!` |
| Bits | `&` `\|` `^` `~` `shl` `shr` |
| Text | `.:` |
| Other | `? :` ternary, `..` range, `=>` lambda, `~>` match arm or signal connection, `.` member, `::` native member, `@` self field or shared table, `...` variadic |

## Literals

Numbers: `42`, `3.5`, `1_000`; strings: `"..."` and `'...'` with `\n`, `\t`, `\\`, `\"`, `\'`; templates: backtick text with `${expression}` interpolation and `\${` for a literal marker; `true`, `false`, `null`.
