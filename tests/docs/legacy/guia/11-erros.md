# 11. Erros

CL++ separa dois tipos de erro:

- **Erros de compilação**: o programa nem começa. Tipos errados, nomes inexistentes, `match` incompleto, função sem `return`. O editor mostra todos enquanto você digita.
- **Erros de execução**: acontecem com o programa rodando. Divisão por zero, índice fora da lista, `report(...)`. Podem ser capturados.

Os dois saem no formato `arquivo:linha:coluna: mensagem`:

```text
jogo.clp:12:9: error: type mismatch
jogo.clp:30:5: runtime error: index out of range: 5 (size 3)
```

## `try` / `catch`

Um erro de execução dentro de `try` desvia para o `catch`, que recebe a mensagem. O programa continua depois do bloco.

```clp
let mut inventory = list("espada");
try {
  post(inventory[3]);
  post("não chega aqui");
} catch (e) {
  post("falhou: " .: e);
}
post("o jogo continua");
```

```saida
falhou: index out of range: 3 (size 1)
o jogo continua
```

O nome entre parênteses é opcional: `catch { ... }` também vale.

Erros dentro de funções chamadas no `try` também são capturados:

```clp
func divide(int a, int b) -> int { return a / b; }
try {
  post(divide(10, 0));
} catch (e) {
  post(e);
}
```

```saida
division by zero
```

## Provocar um erro: `report`

`report(valor)` interrompe a execução com o valor como mensagem. Use para situações que não deveriam acontecer.

```clp
func loadLevel(int id) {
  if (id < 1) {
    report("nível inválido: " .: id);
  }
  post("carregando " .: id);
}
try {
  loadLevel(2);
  loadLevel(0);
} catch (e) {
  post("erro: " .: e);
}
```

```saida
carregando 2
erro: nível inválido: 0
```

Sem `try`, o programa para e o erro é impresso com a posição.

## Avisos: `warn`

`warn(valor)` escreve `warning: valor` na saída de erros (stderr) e **não** interrompe nada. Útil para registrar algo suspeito sem parar o jogo.

```clp
let fps = 24;
if (fps < 30) {
  warn("fps baixo: " .: fps);
}
post("rodando");
```

```saida
rodando
```

## `pcall`: testar se algo falha

`pcall(expressão)` avalia a expressão e devolve `1` se deu certo ou `0` se falhou, sem interromper.

```clp
post(pcall(10 / 2));
post(pcall(10 / 0));
```

```saida
1
0
```

## Quando usar cada um

| Situação | Ferramenta |
| --- | --- |
| Falha esperada, com motivo, que quem chama deve tratar | `Result<T, E>` (capítulo 8) |
| Valor que pode não existir | `Option<T>` (capítulo 8) |
| Algo que nunca deveria acontecer | `report(...)` |
| Proteger um trecho que pode falhar | `try` / `catch` |
| Registrar sem parar | `warn(...)` |

## Erros de execução da linguagem

| Mensagem | Causa |
| --- | --- |
| `division by zero` | `/` ou `%` por zero |
| `index out of range: i (size n)` | índice fora de uma lista ou texto |
| `pop from an empty list` | `pop` numa lista vazia |
| `not a number: "x"` | `Text.toNumber` com texto inválido |
| `stack overflow` | recursão sem fim (mais de 200 000 chamadas aninhadas) |
| `type error` | operação com um valor de tipo inesperado (normalmente pego na compilação) |

## Erros de compilação mais comuns

| Mensagem | Significado |
| --- | --- |
| `undefined name` / `undefined function` | nome não declarado (ou declarado em outro escopo) |
| `type mismatch` | tipo do valor não combina com o esperado |
| `cannot assign to immutable binding` | mudou uma variável `let`; use `let mut` |
| `wrong number of arguments` | quantidade de argumentos diferente da declaração |
| `missing return` | algum caminho da função termina sem `return` |
| `non-exhaustive match` | faltou tratar um caso do enum |
| `unknown field` | o struct não tem esse campo |
| `private member` | acesso a campo `private` fora do struct |
| `abstract type` | tentou construir um `abstract struct` |
| `cannot open module` | o arquivo do `link` não foi encontrado |
