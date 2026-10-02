# 2. Léxico: comentários, nomes e literais

Este capítulo cobre as peças que o compilador reconhece antes de entender a gramática: comentários, identificadores, palavras-chave e literais.

## Comentários

CL++ usa `<<` para comentários de linha e `<<[ … ]>>` para comentários de bloco.

```clp
<< comentário de linha: vai até o fim da linha
post(1); << também pode ficar depois de um comando

<<[
  comentário de bloco:
  pode ocupar várias linhas
]>>
post(2);
```

```saida
1
2
```

Arquivos com fim de linha do Windows (CRLF) ou do macOS antigo (CR) funcionam igual: o comentário de linha termina em qualquer um deles.

### Diretivas

Um comentário que começa com `<<!` no início do arquivo é uma diretiva para o verificador de tipos:

| Diretiva | Efeito |
| --- | --- |
| `<<!strict` | tipos literais e uniões são verificados com rigor (ver capítulo 8) |
| `<<!nocheck` | desliga a verificação de tipos do arquivo (útil para protótipos) |

## Identificadores

Nomes começam com letra ou `_` e continuam com letras, dígitos ou `_`. Maiúsculas e minúsculas são diferentes: `score` e `Score` são nomes distintos.

Convenção usada na biblioteca e nos exemplos:

- `camelCase` ou `snake_case` para variáveis e funções;
- `PascalCase` para structs, enums, variants e aliases de módulo (`Player`, `Axiom`).

## Palavras-chave

As palavras abaixo são reservadas e não podem ser usadas como nomes.

| Grupo | Palavras |
| --- | --- |
| Declarações | `let`, `mut`, `const`, `constexpr`, `auto`, `func`, `struct`, `enum`, `variant`, `type`, `namespace`, `link`, `import`, `using`, `as`, `extern`, `static`, `signal`, `observable`, `atomic`, `operator` |
| Modificadores | `abstract`, `override`, `private`, `public`, `final`, `async` |
| Controle | `if`, `else`, `while`, `for`, `in`, `break`, `continue`, `return`, `match`, `guard`, `switch`, `case`, `default`, `try`, `catch` |
| Concorrência | `spawn`, `await`, `parallel`, `thread`, `join`, `task` |
| Operadores em palavra | `and`, `or`, `not`, `shl`, `shr` |
| Saída | `post`, `warn`, `report`, `cout`, `endl` |
| Tipos | `int`, `float`, `double`, `bool`, `string`, `void`, `buffer`, `Vector2`, `Vector3`, `Vector4` |
| Outros | `true`, `false`, `null`, `new`, `move`, `decltype`, `pcall`, `where`, `from` |

A lista completa e sempre atualizada é a que o editor sugere ao digitar: ela vem direto do compilador.

## Literais

### Números

Inteiros e decimais usam a notação usual. Um número com ponto é `float`; sem ponto é `int`.

```clp
post(42);
post(3.5);
post(-7);
post(0.1 + 0.2);
```

```saida
42
3.5
-7
0.30000000000000004
```

Números inteiros sempre são impressos sem casas decimais. Decimais são impressos com a menor quantidade de dígitos que representa o valor exato, sem arredondar escondido.

### Booleanos e nulo

`true` e `false` são do tipo `bool`. `null` representa "nenhum valor".

```clp
post(true);
post(not true);
post(3 > 2);
```

```saida
true
false
true
```

### Textos

Textos ficam entre aspas duplas ou simples. As sequências de escape são `\n` (nova linha), `\t` (tabulação), `\\`, `\"` e `\'`.

```clp
post("aspas \"duplas\"");
post('aspas simples');
post("coluna1\tcoluna2");
```

```saida
aspas "duplas"
aspas simples
coluna1	coluna2
```

### Templates

Um texto entre crases é um template: cada `${expressão}` é avaliado e inserido no lugar. Para escrever `${` literalmente, use `\${`.

```clp
let name = "Ada";
let level = 3;
post(`${name} está no nível ${level}`);
post(`próximo nível: ${level + 1}`);
post(`literal: \${name}`);
```

```saida
Ada está no nível 3
próximo nível: 4
literal: ${name}
```

## Ponto e vírgula e blocos

Todo comando termina com `;`. Blocos ficam entre `{` e `}` e criam um novo escopo: uma variável declarada dentro de um bloco deixa de existir quando ele termina.

```clp
let x = 1;
if (true) {
  let x = 2;  << outro x, só dentro do bloco
  post(x);
}
post(x);
```

```saida
2
1
```
