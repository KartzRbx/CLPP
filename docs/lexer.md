# Lexer

Referência técnica do analisador léxico (`src/core/lexer/lexer.cpp`). Para o uso da linguagem, veja o [guia, capítulo 2](guia/02-lexico.md).

## Entrada e saída

`Lexer(const std::string&)` recebe o código; `tokenize()` devolve `LexResult` com `tokens` e `diagnostics`. Um construtor com `std::string&&` é deletado de propósito: tokens guardam `std::string_view` sobre o texto, então o texto precisa viver mais que eles (passar uma string temporária era uso de memória após liberar).

Erros léxicos não param a análise: o trecho vira um token `Invalid` e um `Diagnostic` (`location`, `span`, `message`), e o parser continua.

## Fim de linha

`\n`, `\r\n` e `\r` são todos fim de linha (`at_line_end()`). Antes da revisão, um comentário de linha num arquivo CRLF engolia o resto do arquivo.

## Comentários

| Forma | Exemplo |
| --- | --- |
| linha | `<< texto` |
| bloco | `<<[ texto ]>>` |
| documentação | `<<[[]] texto >>` |
| diretiva | `<<!strict`, `<<!nocheck` (início do arquivo) |

`//` não é comentário: são dois tokens `/`.

## Literais

| Forma | Token |
| --- | --- |
| `42`, `3.5`, `1e3`, `0xFF`, `0b1010` | `Number` |
| `"…"`, `'…'` | `StringLiteral` (escapes `\n \t \\ \" \'`) |
| `` `…${expr}…` `` | `TemplateString` (o parser expande `${}`; `\${` é literal) |

`_` separa dígitos apenas entre dois dígitos do mesmo componente: `10_000`, `0xFF_00_FF` são válidos; `10_`, `1__0`, `1_.5`, `0x_FF` são `invalid numeric separator`.

Decimais são lidos com `std::from_chars` (ou `istringstream` com locale clássico onde a biblioteca padrão não tem `from_chars` para `double`), então `3.5` é `3.5` em qualquer idioma do sistema.

## Operadores

`+ - * / %`, `= += -= *= /= %= ++ --`, `== != < <= > >=`, `&& || !`, `& | ^ ~`, `.:` (concatenação), `.` `..` `...`, `::`, `~>`, `=>`, `? :`, `@`, `( ) { } [ ] ; ,`.

Palavras que também são operadores: `and`, `or`, `not`, `shl`, `shr`.

## Palavras-chave

Tabela estática ordenada com busca binária (`std::lower_bound`), sem alocação por identificador. `keyword_names()` expõe a lista para o editor (autocompletar e teste da gramática). Lista completa em [keywords.md](keywords.md).

`Vector2`, `Vector3`, `Vector4` e `buffer` são tokens de tipo dedicados; `Vector3(…)` e `buffer::create(…)` são chamadas.

## `@`

O lexer sempre emite `@` como token separado. O significado vem depois:

| Contexto | Exemplo | Significado |
| --- | --- | --- |
| depois de `link` | `link @clpp.axiom` | módulo da biblioteca padrão |
| dentro de método, nome de campo | `@hp`, `@this.hp`, `@this::hp` | `self.hp` |
| fora de método | `@coins` | tabela compartilhada do programa |

## Diagnósticos

| Situação | Mensagem |
| --- | --- |
| texto sem fecho ou com quebra de linha no meio | `unterminated string` |
| template sem fecho | `unterminated template string` |
| `<<[` sem `]>>` | `unterminated block comment` |
| `<<[[]]` sem `>>` | `unterminated doc comment` |
| `_` fora de lugar | `invalid numeric separator` |
| `0x`, `0b` ou expoente sem dígito | `invalid numeric literal` |
| outro caractere | `unexpected character` |

Texto ou template interrompido por quebra de linha não engole o resto do arquivo.
