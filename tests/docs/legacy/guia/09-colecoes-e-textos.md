# 9. Coleções e textos

## Listas

`list(...)` cria uma lista com qualquer tipo de valor. Os índices começam em 0.

```clp
let mut inventory = list("espada", "poção", "mapa");
post(inventory[0]);
post(len(inventory));
inventory[1] = "elixir";
post(inventory);
```

```saida
espada
3
["espada", "elixir", "mapa"]
```

Um índice fora da lista é um erro em tempo de execução com a posição e o tamanho na mensagem (por exemplo `index out of range: 5 (size 3)`), capturável com `try`/`catch`.

### Adicionar e remover

Estas funções alteram a variável passada como primeiro argumento (que precisa ser mutável).

| Função | Efeito | Devolve |
| --- | --- | --- |
| `push(xs, v)` | adiciona `v` no fim | nada |
| `pop(xs)` | remove o último | o elemento removido |
| `insert(xs, i, v)` | insere `v` na posição `i` | nada |
| `remove(xs, i)` | remove a posição `i` | o elemento removido |

```clp
let mut queue = list();
push(queue, "ana");
push(queue, "bia");
push(queue, "caio");
insert(queue, 0, "zé");
post(queue);
post(remove(queue, 1));
post(pop(queue));
post(queue);
```

```saida
["zé", "ana", "bia", "caio"]
ana
caio
["zé", "bia"]
```

### Buscar, ordenar, percorrer

| Função | Efeito |
| --- | --- |
| `len(xs)` | quantidade de elementos |
| `find(xs, v)` | posição de `v`, ou `-1` |
| `sort(xs)` | nova lista de números em ordem crescente |
| `for (let x in xs)` | percorre os elementos (capítulo 5) |

```clp
let scores = list(40, 95, 70);
post(sort(scores));
post(find(scores, 95));
post(find(scores, 12));
let mut total = 0;
for (let s in scores) { total += s; }
post(total / len(scores));
```

```saida
[40, 70, 95]
1
-1
68
```

### Listas tipadas: `array<T>`

`array<T>` diz ao compilador o tipo dos elementos. Isso permite acessar campos de structs dentro da lista e pega erros de tipo cedo.

```clp
struct Enemy { string name; int hp; }
array<Enemy> wave = array<Enemy>(Enemy("slime", 10), Enemy("orc", 40));
wave[0].hp -= 4;
for (let e in wave) {
  post(e.name .: ": " .: e.hp);
}
```

```saida
slime: 6
orc: 40
```

### Desmontar

```clp
let (x, y) = list(3, 4);
post(x * y);
```

```saida
12
```

## Dicionários

`dictionary<K, V>(chave1, valor1, chave2, valor2, ...)` associa chaves a valores.

```clp
dictionary<string, int> prices = dictionary<string, int>("espada", 150, "poção", 20);
post(prices["poção"]);
prices["escudo"] = 90;     << chave nova
prices["espada"] -= 30;    << chave existente
post(prices);
post(len(prices));
for (let item in prices) {
  post(item .: " custa " .: prices[item]);
}
post(remove(prices, "poção"));
post(prices);
```

```saida
20
{espada: 120, poção: 20, escudo: 90}
3
espada custa 120
poção custa 20
escudo custa 90
20
{espada: 120, escudo: 90}
```

A ordem dos itens é a ordem em que as chaves foram inseridas.

## Vetores

`Vector2`, `Vector3` e `Vector4` são tipos de valor para posições, direções, velocidades e cores. Os componentes são `x`, `y`, `z`, `w` (ou índices `0` a `3`).

```clp
Vector3 position = Vector3(1, 2, 3);
position.x += 10;
post(position);
post(position.y);
post(position[2]);
post(Vector2(8, 9));
post(Vector4(1, 0, 0, 1).w);
```

```saida
(11, 2, 3)
2
3
(8, 9)
1
```

Matemática de vetores (distância, normalização, interpolação, ângulo) está na biblioteca Axiom (capítulo 14).

## Buffers

Um `buffer` é um bloco de bytes de tamanho fixo, útil para pacotes de rede e arquivos binários.

```clp
buffer packet = buffer::create(64);
buffer::write_string(packet, 0, "CL++");
post(buffer::size(packet));
```

```saida
64
```

## Textos

Textos são imutáveis e em UTF-8: índices, `len` e `for … in` contam caracteres, não bytes.

```clp
let word = "coração";
post(len(word));
post(word[4]);
post(word[0 .. 4]);
post(word .: "!");
```

```saida
7
ç
cora
coração!
```

### Biblioteca de texto

`link @clpp.text as Text;` traz as funções de texto.

| Função | Exemplo | Resultado |
| --- | --- | --- |
| `Text.trim(s)` | `Text.trim("  oi ")` | `"oi"` |
| `Text.upper(s)` / `Text.lower(s)` | `Text.upper("abc")` | `"ABC"` |
| `Text.contains(s, parte)` | `Text.contains("abc", "b")` | `true` |
| `Text.startsWith(s, p)` / `Text.endsWith(s, p)` | `Text.endsWith("a.png", ".png")` | `true` |
| `Text.indexOf(s, parte)` | `Text.indexOf("abc", "c")` | `2` (ou `-1`) |
| `Text.split(s, sep)` | `Text.split("a,b", ",")` | `["a", "b"]` |
| `Text.joinAll(lista, sep)` | `Text.joinAll(list("a", "b"), "-")` | `"a-b"` |
| `Text.replace(s, de, para)` | `Text.replace("1-2", "-", "+")` | `"1+2"` |
| `Text.repeat(s, n)` | `Text.repeat("ab", 2)` | `"abab"` |
| `Text.toNumber(s)` | `Text.toNumber("4.5")` | `4.5` (erro se não for número) |

```clp
link @clpp.text as Text;

let line = "  nome=Ada;nivel=7  ";
let fields = Text.split(Text.trim(line), ";");
for (let field in fields) {
  let pair = Text.split(field, "=");
  post(Text.upper(pair[0]) .: " -> " .: pair[1]);
}
let level = Text.toNumber(Text.split(fields[1], "=")[1]);
post(level + 1);
```

```saida
NOME -> Ada
NIVEL -> 7
8
```

### Juntar valores em texto

`.:` aceita qualquer valor (capítulo 4). Templates (capítulo 2) costumam ser mais legíveis para frases com vários valores:

```clp
let name = "Ada";
let pos = Vector2(3, 4);
post(`${name} está em ${pos}`);
```

```saida
Ada está em (3, 4)
```
