# 8. Enums, variants, Option, Result e tipos união

Estes tipos descrevem "um valor que pode ser uma coisa entre várias". Combinados com `match`, eles fazem o compilador conferir que todos os casos foram tratados.

## Enums

Uma lista fechada de casos com nome.

```clp
enum Direction { North, East, South, West }

let facing = Direction.East;
if (facing == Direction.East) {
  post("indo para leste");
}
post(Direction.West);   << cada caso é numerado a partir de 0
```

```saida
indo para leste
3
```

Com `match`, o compilador exige que todos os casos apareçam (capítulo 5).

```clp
enum Weather { Sun, Rain, Snow }
func speedFactor(Weather w) -> float {
  match (w) {
    Sun ~> return 1.0;
    Rain ~> return 0.8;
    Snow ~> return 0.5;
  }
}
post(speedFactor(Weather.Rain));
```

```saida
0.8
```

## Enums com dados

Cada caso pode carregar um valor. O `match` extrai o valor com um nome entre parênteses.

```clp
enum Event { Damage(int), Heal(int), Say(string) }

func handle(Event e) {
  match (e) {
    Damage(amount) ~> post("dano " .: amount);
    Heal(amount) ~> post("cura " .: amount);
    Say(text) ~> post("fala: " .: text);
  }
}
handle(Event.Damage(12));
handle(Event.Say("olá"));
```

```saida
dano 12
fala: olá
```

## `Option<T>`: um valor que pode faltar

`Some(valor)` ou `None`. Substitui o "valor mágico" (como `-1` ou `null`) por algo que o compilador obriga a verificar.

```clp
func findItem(string name) -> Option<int> {
  if (name == "espada") { return Some(150); }
  return None;
}

match (findItem("espada")) {
  Some(price) ~> post("preço " .: price);
  None ~> post("não existe");
}
match (findItem("dragão")) {
  Some(price) ~> post("preço " .: price);
  None ~> post("não existe");
}
```

```saida
preço 150
não existe
```

## `Result<T, E>`: sucesso ou erro

`Ok(valor)` ou `Err(motivo)`. Útil para operações que podem falhar de jeito esperado (carregar um save, validar uma entrada).

```clp
func parseLevel(int raw) -> Result<int, string> {
  if (raw < 1) { return Err("nível precisa ser positivo"); }
  if (raw > 99) { return Err("nível máximo é 99"); }
  return Ok(raw);
}

match (parseLevel(120)) {
  Ok(level) ~> post("nível " .: level);
  Err(reason) ~> post("erro: " .: reason);
}
```

```saida
erro: nível máximo é 99
```

## Variants

Um `variant` guarda um valor de um entre vários tipos.

```clp
variant Value { int, string }
let a = Value(7);
let b = Value("sete");
post(a);
post(b);
```

```saida
7
sete
```

## Tipos união e literais

`type` dá nome a um tipo. Uma união de textos literais descreve um conjunto fechado de estados, verificado em compilação.

```clp
<<!strict
type State = "Idle" | "Running" | "Jumping";
let state: State = "Running";
if (state == "Running") {
  post("correndo");
}
```

```saida
correndo
```

No modo `<<!strict`, atribuir ou comparar com um texto fora da união é erro:

```clp erro
<<!strict
type State = "Idle" | "Running";
let state: State = "Dead";
```

```saida
type mismatch
```

`type A = B & C;` declara uma interseção: o valor precisa satisfazer os dois.

## Quando usar cada um

| Situação | Use |
| --- | --- |
| Conjunto fixo de casos sem dados | `enum` |
| Casos que carregam dados diferentes | `enum` com dados |
| Valor que pode não existir | `Option<T>` |
| Operação que pode falhar com motivo | `Result<T, E>` |
| Um valor de um entre poucos tipos | `variant` |
| Estados nomeados por texto, verificados | `type X = "a" \| "b"` com `<<!strict` |
