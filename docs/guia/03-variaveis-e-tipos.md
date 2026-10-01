# 3. Variáveis e tipos

CL++ é estaticamente tipada com inferência: o compilador descobre o tipo de cada variável e recusa operações que não fazem sentido antes de o programa rodar. Você só escreve o tipo quando quer, ou quando ele não pode ser deduzido.

## Três jeitos de declarar

| Forma | Mutável? | Tipo |
| --- | --- | --- |
| `let nome = valor;` | não | inferido |
| `let mut nome = valor;` | sim | inferido |
| `Tipo nome = valor;` | sim | o escrito |

```clp
let gravity = 9.8;        << imutável
let mut speed = 0;        << pode mudar
int lives = 3;            << tipo escrito: pode mudar
speed = speed + 5;
lives = lives - 1;
post(gravity);
post(speed);
post(lives);
```

```saida
9.8
5
2
```

Tentar mudar uma variável `let` é erro de compilação. O editor oferece a correção rápida **Tornar 'x' mutável (let mut)**.

```clp erro
let hp = 100;
hp = 50;
```

```saida
cannot assign to immutable binding
```

### Anotação de tipo com `let`

Também dá para escrever o tipo depois do nome, com `:`.

```clp
let name: string = "Ada";
let mut level: int = 1;
level = level + 1;
post(name .: " " .: level);
```

```saida
Ada 2
```

## Constantes

`const` declara um valor fixo. `constexpr` exige que o valor seja calculável na compilação, a partir de literais.

```clp
const MAX_HP = 100;
constexpr SECONDS_PER_HOUR = 60 * 60;
post(MAX_HP);
post(SECONDS_PER_HOUR);
```

```saida
100
3600
```

Constantes de topo também podem ser lidas dentro de funções e são exportadas por módulos (capítulo 10).

## Tipos primitivos

| Tipo | Valores | Exemplo |
| --- | --- | --- |
| `int` | inteiros | `42`, `-7` |
| `float` / `double` | decimais (64 bits) | `3.5`, `0.016` |
| `bool` | `true`, `false` | `hp > 0` |
| `string` | texto UTF-8 | `"Ada"` |
| `Vector2`, `Vector3`, `Vector4` | vetores matemáticos | `Vector3(0, 1, 0)` |
| `buffer` | bloco de bytes | `buffer::create(256)` |
| `void` | sem valor (retorno de função) | `func f() -> void` |

`int` e `float` convivem: um `int` pode ser usado onde se espera `float`. A divisão entre dois `int` é inteira (corta a parte decimal); basta um `float` para ela ser decimal.

```clp
post(7 / 2);
post(7.0 / 2);
int a = 9;
float b = a;
post(b / 2);
```

```saida
3
3.5
4.5
```

## `auto` e `decltype`

`auto` pede ao compilador que deduza o tipo, como `let mut`. `decltype(x)` usa o tipo de outra variável.

```clp
auto base = 3;
decltype(base) other = 4;
post(base + other);
```

```saida
7
```

## Tipos compostos

Estes são cobertos nos próximos capítulos:

| Tipo | Capítulo |
| --- | --- |
| `struct` (com herança) | 7 |
| `enum`, `variant`, `Option<T>`, `Result<T, E>`, `type` (uniões) | 8 |
| `list`, `array<T>`, `dictionary<K, V>` | 9 |
| `task` | 12 |

## Semântica de valor

Structs, listas e vetores são **valores**: atribuir ou passar para uma função faz uma cópia. Mudar a cópia não muda o original. Isso evita uma classe inteira de bugs em jogos (dois sistemas mexendo no mesmo objeto sem saber).

```clp
struct Stats { int hp; }
Stats a = Stats(100);
Stats b = a;      << cópia
b.hp = 1;
post(a.hp);
post(b.hp);
```

```saida
100
1
```

Para alterar o próprio objeto, use um método que muda `self` (capítulo 7) ou atribua o resultado de volta.

## Mover em vez de copiar

`move(x)` entrega o valor de `x` e deixa `x` vazio, sem copiar. Útil para textos e listas grandes.

```clp
let mut text = "um texto longo";
let taken = move(text);
post(taken);
post(text);
```

```saida
um texto longo

```
