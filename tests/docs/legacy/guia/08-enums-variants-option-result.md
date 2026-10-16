# 8. Enums, opcionais, exceções e uniões

Estes tipos descrevem "um valor que pode ser uma coisa entre várias". Combinados com `match`, eles fazem o compilador conferir que todos os casos foram tratados.

## Enums

Uma lista fechada de casos com nome.

```clp
enum class Direction { North, East, South, West }

const auto facing = Direction::East;
if (facing == Direction::East) {
  post("indo para leste");
}
post(Direction::West);   << cada caso é numerado a partir de 0
```

```saida
indo para leste
3
```

Com `match`, o compilador exige que todos os casos apareçam (capítulo 5).

```clp
enum class Weather { Sun, Rain, Snow }
float speedFactor(Weather w) {
  match (w) {
    Weather::Sun => return 1.0;
    Weather::Rain => return 0.8;
    Weather::Snow => return 0.5;
  }
}
post(speedFactor(Weather::Rain));
```

```saida
0.8
```

## Casos com dados

Use um enum com escopo e um registro com campos explícitos.

```clp
enum class EventKind { Damage, Heal, Say }
def Event { EventKind Kind; number Amount = 0; string Text = ""; }
void handle(Event e) {
  match (e.Kind) {
    EventKind::Damage => post("dano " .: e.Amount);
    EventKind::Heal => post("cura " .: e.Amount);
    EventKind::Say => post("fala: " .: e.Text);
  }
}
handle(Event { Kind = EventKind::Damage, Amount = 12 });
handle(Event { Kind = EventKind::Say, Text = "olá" });
```

```saida
dano 12
fala: olá
```

## Optional values

Use `optional<T>` and `null`; check for absence before reading the contained value.

```clp
optional<number> FindItem(string name) {
  if (name == "sword") { return 150; }
  return null;
}
void Show(optional<number> price) {
  guard (price != null) else { post("missing"); return; }
  post("price " .: price);
}
Show(FindItem("sword"));
Show(FindItem("dragon"));
```

```output
price 150
missing
```

## Exceptions

`report` raises an error; `try` / `catch` handles it.

```clp
number ParseLevel(number raw) {
  if (raw < 1) { report("level must be positive"); }
  if (raw > 99) { report("maximum level is 99"); }
  return raw;
}
try { post(ParseLevel(120)); } catch (reason) { post("error: " .: reason); }
```

```output
error: maximum level is 99
```

## Unions

Name a union with `def`.

```clp
def Value = number | string;
Value a = 7;
Value b = "seven";
post(a);
post(b);
```

```output
7
seven
```

## Tipos união e literais

`def` dá nome a um tipo. Uma união de textos literais descreve um conjunto fechado de estados, verificado em compilação.

```clp
pragma strict;
def State = "Idle" | "Running" | "Jumping";
const State state = "Running";
if (state == "Running") {
  post("correndo");
}
```

```saida
correndo
```

No modo `pragma strict;`, atribuir ou comparar com um texto fora da união é erro:

```clp erro
pragma strict;
def State = "Idle" | "Running";
const State state = "Dead";
```

```saida
type mismatch
```

`def A = B & C;` declara uma interseção: o valor precisa satisfazer os dois.

## Quando usar cada um

| Situação | Use |
| --- | --- |
| Conjunto fixo de casos sem dados | `enum class` |
| Casos que carregam dados diferentes | `enum class` e `def` com campos |
| Valor que pode não existir | `optional<T>` e `null` |
| Operação que pode falhar com motivo | `try` / `catch` |
| Um valor de um entre poucos tipos | `def Value = A | B;` |
| Estados nomeados por texto, verificados | `def X = "a" \| "b"` com `pragma strict;` |
