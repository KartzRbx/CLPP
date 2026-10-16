# 3. Variáveis e tipos

CL++ é estaticamente tipada com inferência: o compilador descobre o tipo de cada variável e recusa operações que não fazem sentido antes de o programa rodar. Você só escreve o tipo quando quer, ou quando ele não pode ser deduzido.

## Três jeitos de declarar

| Forma | Mutável? | Tipo |
| --- | --- | --- |
| `let nome = valor;` | não | inferido |
| `let mut nome = valor;` | sim | inferido |
| `Tipo nome = valor;` | sim | o escrito |

```clp
const auto gravity = 9.8;        << imutável
auto speed = 0;        << pode mudar
number lives = 3;            << tipo escrito: pode mudar
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
const auto hp = 100;
hp = 50;
```

```saida
cannot assign to immutable binding
```

### Anotação de tipo com `let`

Também dá para escrever o tipo depois do nome, com `:`.

```clp
const string name = "Ada";
number level = 1;
level = level + 1;
post(name .: " " .: level);
```

```saida
Ada 2
```

## Constantes

`const` declara um valor fixo. `constexpr` exige que o valor seja calculável na compilação, a partir de literais.

```clp
const auto MAX_HP = 100;
constexpr number SECONDS_PER_HOUR = 60 * 60;
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
| `number` | inteiros | `42`, `-7` |
| `float` / `float` | decimais (64 bits) | `3.5`, `0.016` |
| `bool` | `true`, `false` | `hp > 0` |
| `string` | texto UTF-8 | `"Ada"` |
| `Vector2`, `Vector3`, `Vector4` | vetores matemáticos | `Vector3(0, 1, 0)` |
| `buffer` | bloco de bytes | `buffer::create(256)` |
| `void` | sem valor (retorno de função) | `func f() -> void` |

`number` e `float` convivem: um `number` pode ser usado onde se espera `float`. A divisão entre dois `number` é inteira (corta a parte decimal); basta um `float` para ela ser decimal.

```clp
post(7 / 2);
post(7.0 / 2);
number a = 9;
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
| `enum class`, `optional<T>`, exceções e uniões com `def` | 8 |
| `list`, `array<T>`, `dictionary<K, V>` | 9 |
| `task` | 12 |

## Referências e valores

Objetos `def`, arrays e dictionaries compartilham identidade. Atribuir ou passar um objeto preserva a mesma referência; números e vetores continuam sendo valores.

```clp
def Stats { number hp; }
Stats a = Stats { hp = 100 };
Stats b = a; << mesma referência
b.hp = 1;
post(a.hp);
post(b.hp);
```

```saida
1
1
```

Alterações por campos, parâmetros e métodos são vistas por todos os aliases. Construa outro objeto quando precisar de estado independente.

## Mover em vez de copiar

`move(x)` entrega o valor de `x` e deixa `x` vazio, sem copiar. Útil para textos e listas grandes.

```clp
void DemonstrateMove() {
  auto text = "um texto longo";
  const auto taken = move(text);
  post(taken);
  post(text);
}
DemonstrateMove();
```

```saida
um texto longo

```
