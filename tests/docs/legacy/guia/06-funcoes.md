# 6. Funções

## Declarar e chamar

```clp
func add(int a, int b) -> int {
  return a + b;
}
post(add(2, 3));
```

```saida
5
```

- Os tipos dos parâmetros (`int a`) e do retorno (`-> int`) são opcionais. Sem eles, o compilador infere o que puder.
- Funções podem ser declaradas depois de serem usadas: não existe declaração antecipada nem arquivo de cabeçalho.
- `-> void` (ou nada) indica que a função não devolve valor.

```clp
post(square(7));   << usada antes da declaração

func square(int n) -> int { return n * n; }

func log(string message) -> void {
  post("[log] " .: message);
}
log("pronto");
```

```saida
49
[log] pronto
```

O compilador confere que toda função com tipo de retorno devolve um valor em todos os caminhos, e avisa código inalcançável depois de um `return`.

```clp erro
func sign(int n) -> int {
  if (n > 0) { return 1; }
}
```

```saida
missing return
```

## Parâmetros com valor padrão

```clp
func greet(string name = "mundo", string mark = "!") -> string {
  return "olá, " .: name .: mark;
}
post(greet());
post(greet("Ada"));
```

```saida
olá, mundo!
olá, Ada!
```

## Argumentos nomeados

Qualquer argumento pode ser passado pelo nome, em qualquer ordem. Combina bem com valores padrão: você passa só o que muda.

```clp
func spawnEnemy(string kind = "slime", int level = 1, float speed = 1.0) {
  post(kind .: " nv" .: level .: " vel " .: speed);
}
spawnEnemy(level: 5);
spawnEnemy(speed: 2.5, kind: "lobo");
```

```saida
slime nv5 vel 1
lobo nv1 vel 2.5
```

## Parâmetros variádicos

`...` recebe qualquer quantidade de argumentos como uma lista.

```clp
func total(... values) {
  let mut sum = 0;
  for (let v in values) {
    sum = sum + v;
  }
  return sum;
}
post(total(1, 2, 3, 4));
post(total());
```

```saida
10
0
```

## Recursão

A pilha de chamadas aguenta recursão profunda (o limite padrão é 200 000 chamadas). Passando do limite, o erro é capturável, não um travamento.

```clp
func factorial(int n) -> int {
  if (n <= 1) { return 1; }
  return n * factorial(n - 1);
}
post(factorial(10));
```

```saida
3628800
```

## Vários valores de retorno

Devolva uma lista e desmonte-a com `let (a, b) = …`.

```clp
func minMax(int a, int b) {
  return a < b ? list(a, b) : list(b, a);
}
let (low, high) = minMax(9, 4);
post(low .: " " .: high);
```

```saida
4 9
```

## Genéricos

Um parâmetro de tipo entre `< >` deixa a mesma função servir para vários tipos. `where` restringe quais tipos são aceitos.

```clp
func maxOf<T>(T a, T b) -> T {
  return a > b ? a : b;
}
post(maxOf<int>(3, 9));
post(maxOf<float>(2.5, 1.5));

func onlyInt<T>(T value) where T: int {
  return value;
}
post(onlyInt<int>(7));
```

```saida
9
2.5
7
```

## Namespaces

Agrupam funções relacionadas sob um nome.

```clp
namespace Physics {
  func gravity() -> float { return 9.8; }
  func fallSpeed(float seconds) -> float { return 9.8 * seconds; }
}
post(Physics.gravity());
post(Physics.fallSpeed(2));
```

```saida
9.8
19.6
```

## Lambdas

`(parâmetro => expressão)` é uma função curta, escrita no lugar onde é usada.

```clp
post((x => x * x)(6));
```

```saida
36
```

Hoje uma lambda é chamada no ponto em que é escrita; guardar uma função em variável ou passá-la como argumento ainda não é suportado. Para callbacks, use `signal` e `OnChange` (capítulo 13), que aceitam funções nomeadas e funções anônimas.

## Funções externas

`extern func` declara uma função nativa fornecida pelo programa que embute o CL++ (por exemplo, o motor do jogo). Alguns nomes já vêm prontos, como `strlen`.

```clp
extern func strlen(string s);
post(strlen("CL++"));
```

```saida
4
```

## Funções estáticas e atributos

`static func` é aceito para quem vem de C++ e se comporta como uma função comum. Atributos entre `[[ ]]` marcam o comando seguinte para ferramentas e backends (por exemplo `[[server]]`) e não mudam o resultado do programa.

```clp
static func show(int x) { post(x); }
[[server]]
show(3);
```

```saida
3
```

## Assinatura no editor

Ao digitar `(` depois do nome de uma função, o VS Code mostra a assinatura e destaca o parâmetro atual. Passar o mouse sobre o nome mostra a assinatura completa e de qual arquivo ela vem.
