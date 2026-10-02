# 5. Controle de fluxo

## `if` / `else`

A condição fica entre parênteses e o corpo entre chaves (as chaves são obrigatórias, mesmo com um só comando).

```clp
let hp = 35;
if (hp <= 0) {
  post("derrotado");
} else if (hp < 50) {
  post("ferido");
} else {
  post("saudável");
}
```

```saida
ferido
```

Uma condição é verdadeira quando é `true`, um número diferente de zero ou um texto não vazio.

### `if (let …)`

Declara uma variável que só existe dentro do `if` e testa o seu valor.

```clp
if (let bonus = 5) {
  post("bônus de " .: bonus);
}
```

```saida
bônus de 5
```

## `while`

Repete enquanto a condição for verdadeira.

```clp
let mut countdown = 3;
while (countdown > 0) {
  post(countdown);
  countdown--;
}
post("já!");
```

```saida
3
2
1
já!
```

## `for … in`

`for (let x in fonte)` percorre a fonte:

| Fonte | `x` recebe |
| --- | --- |
| um número `n` | `0`, `1`, …, `n - 1` |
| uma lista ou array | cada elemento |
| um texto | cada caractere |
| um dicionário | cada chave |

```clp
for (let i in 3) {
  post("volta " .: i);
}
for (let item in list("espada", "escudo")) {
  post(item);
}
for (let letter in "olá") {
  post(letter);
}
```

```saida
volta 0
volta 1
volta 2
espada
escudo
o
l
á
```

## `for` no estilo C

`for (início; condição; passo)`, para quando você precisa controlar o contador.

```clp
for (let mut i = 10; i > 0; i = i - 4) {
  post(i);
}
```

```saida
10
6
2
```

## `break` e `continue`

`break` sai do laço. `continue` pula para a próxima volta. Os dois funcionam em `while`, `for … in` e `for` no estilo C.

```clp
for (let i in 10) {
  if (i % 2 == 0) {
    continue;
  }
  if (i > 7) {
    break;
  }
  post(i);
}
```

```saida
1
3
5
7
```

## `switch`

Compara um valor com cada `case`. Não existe "cair" para o próximo caso: cada caso termina sozinho. `break` dentro do `switch` é aceito e sai dele. `default` pega o resto.

```clp
let key = 2;
switch (key) {
  case 1: post("pular"); break;
  case 2: post("correr"); break;
  default: post("parado"); break;
}
```

```saida
correr
```

## `match`

`match` é a forma mais poderosa de decisão. Cada braço é `padrão ~> comando;`. O primeiro padrão que combina é executado.

```clp
let score = 85;
match (score) {
  100 ~> post("perfeito");
  _ guard score >= 80 ~> post("ótimo");
  _ guard score >= 50 ~> post("bom");
  _ ~> post("tente de novo");
}
```

```saida
ótimo
```

- Um valor literal (`100`) combina quando é igual.
- `_` combina com qualquer coisa.
- `guard condição` adiciona uma condição extra ao braço.

Com enums, o compilador exige que **todos os casos** sejam tratados (ou que exista `_`). Assim, ao criar um caso novo, o compilador mostra cada `match` que precisa ser atualizado.

```clp
enum State { Idle, Running, Jumping }
let state = State.Running;
match (state) {
  Idle ~> post("parado");
  Running ~> post("correndo");
  Jumping ~> post("no ar");
}
```

```saida
correndo
```

```clp erro
enum State { Idle, Running, Jumping }
let state = State.Idle;
match (state) {
  Idle ~> post("parado");
  Running ~> post("correndo");
}
```

```saida
non-exhaustive
```

`match` também desmonta `Option`, `Result` e enums com dados (capítulo 8).

## `return`

Sai da função atual, opcionalmente com um valor (capítulo 6).
