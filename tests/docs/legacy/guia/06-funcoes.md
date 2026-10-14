# 6. Funções

## Declarar e chamar

```clp
number add(number a, number b) {
  return a + b;
}
post(add(2, 3));
```

```saida
5
```

- Os tipos dos parâmetros (`number a`) e do retorno (`-> number`) são opcionais. Sem eles, o compilador infere o que puder.
- Funções podem ser declaradas depois de serem usadas: não existe declaração antecipada nem arquivo de cabeçalho.
- `-> void` (ou nada) indica que a função não devolve valor.

```clp
post(square(7));   << usada antes da declaração

number square(number n) { return n * n; }

void log(string message) {
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
number sign(number n) {
  if (n > 0) { return 1; }
}
```

```saida
missing return
```

## Argumentos explícitos

```clp
string greet(string name, string mark) {
  return "olá, " .: name .: mark;
}
post(greet("mundo", "!"));
post(greet("Ada", "!"));
```

```saida
olá, mundo!
olá, Ada!
```

## Argumentos posicionais

Forneça todos os argumentos na ordem dos parâmetros. Chamadas com argumentos nomeados e valores padrão em parâmetros são rejeitadas. Para opções, use campos com valores padrão em def.

```clp
void spawnEnemy(string kind, number level, float speed) {
  post(kind .: " nv" .: level .: " vel " .: speed);
}
spawnEnemy("slime", 5, 1.0);
spawnEnemy("lobo", 1, 2.5);
```

```saida
slime nv5 vel 1
lobo nv1 vel 2.5
```

## Parâmetros variádicos

`...` recebe qualquer quantidade de argumentos como uma lista.

```clp
number total(any ... values) {
  auto sum = 0;
  for (auto v in values) {
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
number factorial(number n) {
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
array<number> minMax(number a, number b) {
  return a < b ? array<number>(a, b) : array<number>(b, a);
}
const auto limits = minMax(9, 4);
const auto low = limits[0];
const auto high = limits[1];
post(low .: " " .: high);
```

```saida
4 9
```

## Genéricos

Um parâmetro formal em `template <typename T>` preserva o tipo escolhido na chamada. Operações sobre `T` precisam ser válidas para todos os tipos aceitos; comparação numérica sem restrição não é garantida.

```clp
template <typename T>
T Identity(T value) { return value; }
post(Identity<number>(9));
post(Identity<float>(2.5));

template <typename T : number>
T OnlyInt(T value) { return value; }
post(OnlyInt<number>(7));
```

```saida
9
2.5
7
```

## Funções em módulos

Agrupe funções em um arquivo e exporte as que devem ser públicas.

```clp file=physics.clp
export float gravity() { return 9.8; }
export float fallSpeed(float seconds) { return 9.8 * seconds; }
```

```clp
link "./physics.clp" as Physics;
post(Physics.gravity());
post(Physics.fallSpeed(2));
```

```saida
9.8
19.6
```

## Lambdas

Uma lambda usa `func(parâmetro) -> Tipo { corpo }` e pode ser chamada no lugar onde é escrita.

```clp
post((func(number x) -> number { return x * x; })(6));
```

```saida
36
```

Callbacks podem ser guardados em variáveis, passados como argumentos e retornados, com assinaturas verificadas.

## Funções externas

`extern number strlen(string value);` declara uma função nativa fornecida pelo programa que embute o CL++ (por exemplo, o motor do jogo). Alguns nomes já vêm prontos, como `strlen`.

```clp
extern number strlen(string s);
post(strlen("CL++"));
```

```saida
4
```

## Funções de arquivo

Funções de arquivo usam retorno antes do nome. Não há modificador estático ignorado nem atributos entre colchetes.

```clp
void show(number x) { post(x); }

show(3);
```

```saida
3
```

## Assinatura no editor

Ao digitar `(` depois do nome de uma função, o VS Code mostra a assinatura e destaca o parâmetro atual. Passar o mouse sobre o nome mostra a assinatura completa e de qual arquivo ela vem.
