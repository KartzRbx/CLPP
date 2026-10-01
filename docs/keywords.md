# Palavras-chave e operadores

Referência rápida. A explicação com exemplos está no guia: [léxico](guia/02-lexico.md) e [operadores](guia/04-operadores.md).

## Comentários

- `<<` comentário de linha; `<<[ … ]>>` comentário de bloco.
- `<<!strict` e `<<!nocheck` no início do arquivo são diretivas do verificador de tipos.
- `//` **não** é comentário: são dois operadores de divisão.

## Palavras-chave

`Vector2` `Vector3` `Vector4` `abstract` `and` `as` `async` `atomic` `auto` `await` `bool` `break` `buffer` `case` `catch` `const` `constexpr` `continue` `cout` `decltype` `default` `double` `else` `endl` `enum` `extern` `false` `final` `float` `for` `from` `func` `guard` `if` `import` `in` `int` `join` `let` `link` `match` `move` `mut` `namespace` `new` `not` `null` `observable` `operator` `or` `override` `parallel` `pcall` `post` `private` `public` `report` `return` `shl` `shr` `signal` `spawn` `static` `string` `struct` `switch` `task` `thread` `true` `try` `type` `using` `variant` `void` `warn` `where` `while`

A lista é gerada do lexer (`keyword_names()`); um teste confere que a gramática do VS Code tem todas.

`self` não é palavra reservada: é o nome do primeiro parâmetro implícito dos métodos.

## Funções embutidas (não reservadas)

`len` `list` `push` `pop` `insert` `remove` `find` `sort` `args` `mutex` `lock` `unlock` `fetch_add` `atomic_load` `actor`

## Operadores

| Grupo | Operadores |
| --- | --- |
| Aritméticos | `+` `-` `*` `/` `%` |
| Atribuição | `=` `+=` `-=` `*=` `/=` `%=` `++` `--` |
| Comparação | `==` `!=` `<` `<=` `>` `>=` |
| Lógicos | `and` / `&&`, `or` / `\|\|`, `not` / `!` |
| Bits | `&` `\|` `^` `~` `shl` `shr` |
| Texto | `.:` |
| Outros | `? :` (ternário), `..` (intervalo), `=>` (lambda), `~>` (braço de `match`, conexão de `signal`), `.` (membro), `::` (nativo / `@this::x`), `@` (campo de `self` ou tabela compartilhada), `...` (variádico) |

## Literais

- Números: `42`, `3.5`, `1_000` (sublinhado entre dígitos).
- Textos: `"…"` e `'…'`, com `\n \t \\ \" \'`.
- Templates: `` `…${expressão}…` ``, com `\${` para escrever `${`.
- `true`, `false`, `null`.
