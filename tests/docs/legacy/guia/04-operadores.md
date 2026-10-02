# 4. Operadores

## Aritméticos

| Operador | Significado | Exemplo | Resultado |
| --- | --- | --- | --- |
| `+` | soma | `2 + 3` | `5` |
| `-` | subtração e negação | `5 - 8`, `-x` | `-3` |
| `*` | multiplicação | `4 * 2.5` | `10` |
| `/` | divisão (inteira entre dois `int`) | `7 / 2`, `7.0 / 2` | `3`, `3.5` |
| `%` | resto | `7 % 3` | `1` |

```clp
post(2 + 3 * 4);
post((2 + 3) * 4);
post(7 % 3);
post(-7 / 2);
```

```saida
14
20
1
-3
```

A divisão inteira corta em direção a zero, como em C++ (`-7 / 2` é `-3`). Dividir por zero é um erro em tempo de execução, que pode ser capturado com `try`/`catch` (capítulo 11).

## Atribuição composta, incremento e decremento

```clp
let mut score = 10;
score += 5;
score -= 3;
score *= 2;
score /= 4;
score %= 4;
post(score);
score++;
score--;
score++;
post(score);
```

```saida
2
3
```

As formas compostas também valem para campos e elementos: `player.hp -= 10;`, `xs[2] *= 3;` (capítulos 7 e 9).

## Comparação

`==`, `!=`, `<`, `<=`, `>`, `>=` produzem `bool`. Textos são comparados pelo conteúdo.

```clp
post(3 == 3);
post("abc" != "abd");
post(2 >= 5);
```

```saida
true
true
false
```

## Lógicos

As formas em palavra e em símbolo são equivalentes; use a que preferir.

| Palavra | Símbolo | Significado |
| --- | --- | --- |
| `and` | `&&` | e |
| `or` | `\|\|` | ou |
| `not` | `!` | negação |

`and` e `or` são de curto-circuito: o lado direito só é avaliado se for necessário.

```clp
let hp = 30;
let shield = false;
post(hp > 0 and not shield);
post(hp > 50 || shield);
post(0 and (1 / 0));   << o lado direito nunca roda
```

```saida
true
false
false
```

## Bits

Operam sobre a representação inteira do número.

| Operador | Significado | Exemplo | Resultado |
| --- | --- | --- | --- |
| `&` | e bit a bit | `6 & 3` | `2` |
| `\|` | ou bit a bit | `1 \| 2` | `3` |
| `^` | ou exclusivo | `5 ^ 1` | `4` |
| `~` | inverte os bits | `~0` | `-1` |
| `shl` | desloca à esquerda | `1 shl 3` | `8` |
| `shr` | desloca à direita | `16 shr 2` | `4` |

```clp
let FLAG_JUMP = 1 shl 0;
let FLAG_DASH = 1 shl 1;
let flags = FLAG_JUMP | FLAG_DASH;
post(flags);
post((flags & FLAG_DASH) != 0);
post(16 shr 2);
```

```saida
3
true
4
```

`^` é ou exclusivo, não potência. Para potência use `Axiom.Pow(2, 10)` (capítulo 14).

## Texto: `.:`

`.:` junta dois valores como texto. Qualquer valor pode entrar: números, booleanos, vetores, listas.

```clp
let name = "Ada";
let hp = 90;
post(name .: " tem " .: hp .: " de vida");
post("vivo: " .: (hp > 0));
post("posição: " .: Vector3(1, 2, 3));
```

```saida
Ada tem 90 de vida
vivo: true
posição: (1, 2, 3)
```

## Ternário

`condição ? se_verdadeiro : se_falso`.

```clp
let hp = 0;
post(hp > 0 ? "vivo" : "derrotado");
```

```saida
derrotado
```

## Fatias e índices

`texto[i]` devolve o caractere na posição `i` (contando de 0, por caractere UTF-8). `x[a .. b]` devolve a fatia de `a` até antes de `b`.

```clp
post("ação"[1]);
post("abcdef"[1 .. 4]);
```

```saida
ç
bcd
```

## Vetores

`+` soma vetores componente a componente e `*` multiplica um vetor por um número.

```clp
Vector3 position = Vector3(0, 10, 0);
Vector3 velocity = Vector3(2, 0, 1);
Vector3 next = position + velocity * 0.5;
post(next);
post(next.y);
```

```saida
(1, 10, 0.5)
10
```

## Precedência

Da mais forte para a mais fraca. Operadores do mesmo nível são avaliados da esquerda para a direita.

| Nível | Operadores |
| --- | --- |
| 1 | chamada `f()`, membro `a.b`, índice `a[i]`, `x++`, `x--` |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-`, `.:`, `..` |
| 4 | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| 5 | `-x`, `~x`, `not`, `!` (aplicados ao que vem à direita) |
| 6 | `shl`, `shr` |
| 7 | `&` |
| 8 | `^` |
| 9 | `\|` |
| 10 | `and`, `&&` |
| 11 | `or`, `\|\|` |
| 12 | `? :` |

Dois casos pedem parênteses:

- `.:` está no mesmo nível de `+`: `"total: " .: a + b` tenta somar texto com número. Escreva `"total: " .: (a + b)`.
- `shl`/`shr` são mais fracos que a comparação: escreva `(1 shl 3) == 8`.

```clp
let a = 2;
let b = 3;
post("total: " .: (a + b));
post((1 shl 3) == 8);
```

```saida
total: 5
true
```

## Operadores para seus tipos

Um struct pode definir o próprio `+` com `func operator+`:

```clp
struct Money { int cents; }
func operator+(Money a, Money b) { return Money(a.cents + b.cents); }
Money total = Money(150) + Money(275);
post(total.cents);
```

```saida
425
```
