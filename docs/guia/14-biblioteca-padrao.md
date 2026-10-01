# 14. Biblioteca padrão

A biblioteca padrão é importada com `link @clpp.<nome>`. Cada módulo é escrito em CL++ sobre funções nativas em C++, então o editor mostra a assinatura de tudo e o compilador confere os tipos.

| Módulo | Nome sugerido | Para quê |
| --- | --- | --- |
| `@clpp.axiom` | `Axiom` | matemática para jogos |
| `@clpp.text` | `Text` | textos (capítulo 9) |
| `@clpp.math` | `Math` | `abs` |
| `@clpp.fs` | `Fs` | arquivos |
| `@clpp.os` | `Os` | variáveis de ambiente |
| `@clpp.http` | `Http` | URLs |

Funções sempre disponíveis, sem `link`: `post`, `warn`, `report`, `len`, `list`, `push`, `pop`, `insert`, `remove`, `find`, `sort`, `args`, `pcall`, e os construtores `Vector2/3/4`, `array<T>`, `dictionary<K, V>`, `buffer::create`.

## Axiom: matemática para jogos

```clp
link @clpp.axiom as Axiom;
post(Axiom.Clamp(150, 0, 100));
post(Axiom.Lerp(0, 100, 0.25));
post(Axiom.Distance(Vector3(0, 0, 0), Vector3(3, 4, 0)));
post(Axiom.OutQuad(0.5));
```

```saida
100
25
5
0.75
```

### Números

| Função | O que faz | Exemplo |
| --- | --- | --- |
| `Clamp(x, min, max)` | limita `x` ao intervalo | `Clamp(150, 0, 100)` → `100` |
| `Saturate(x)` | limita a `[0, 1]` | |
| `Wrap(x, min, max)` | dá a volta no intervalo | `Wrap(370, 0, 360)` → `10` |
| `PingPong(x, length)` | vai e volta entre 0 e `length` | `PingPong(7, 5)` → `3` |
| `Snap(x, step)` | arredonda para o múltiplo de `step` | `Snap(17, 5)` → `15` |
| `Round(x)`, `Round(x, casas)` | arredonda | `Round(3.14159, 2)` → `3.14` |
| `Floor`, `Ceil`, `Trunc`, `Fract` | partes inteira e fracionária | `Floor(-2.5)` → `-3` |
| `Sign(x)`, `Abs(x)` | sinal e valor absoluto | |
| `Min(a, b)`, `Max(a, b)` | menor e maior | |
| `Pow(b, e)`, `Sqrt`, `Cbrt`, `Hypot(a, b)` | potência e raízes | `Pow(2, 10)` → `1024` |
| `Log`, `Log2`, `Log10`, `Exp` | logaritmos | |
| `Gcd`, `Lcm`, `Factorial` | aritmética inteira | `Factorial(5)` → `120` |
| `IsEven`, `IsOdd`, `IsFinite`, `IsNaN` | testes (devolvem `bool`) | |
| `Average(lista)`, `Sum(lista)` | média e soma | `Average(list(2, 4, 9))` → `5` |
| `Pi()` | π | |

### Interpolação

| Função | O que faz |
| --- | --- |
| `Lerp(a, b, t)` | de `a` (t = 0) até `b` (t = 1) |
| `LerpClamped(a, b, t)` | `Lerp` com `t` limitado a `[0, 1]` |
| `InvLerp(a, b, x)` | o `t` em que `x` está entre `a` e `b` |
| `Map(x, a1, b1, a2, b2)` | leva `x` do intervalo 1 para o intervalo 2 |
| `Approach(atual, alvo, passo)` | anda até `passo` em direção ao alvo, sem passar |
| `Smoothstep(a, b, x)`, `Smootherstep(a, b, x)` | transição suave |
| `LerpAngle(a, b, t)` | interpola ângulos pelo caminho mais curto (radianos) |
| `Scale(x, fator)`, `Inverse(a, b, x)` | escala e inversa |

```clp
link @clpp.axiom as Axiom;
let mut speed = 10;
speed = Axiom.Approach(speed, 20, 3);
post(speed);
post(Axiom.Map(5, 0, 10, 0, 100));
post(Axiom.InvLerp(0, 100, 25));
```

```saida
13
50
0.25
```

### Vetores

| Função | O que faz |
| --- | --- |
| `Dot(a, b)` | produto escalar |
| `Cross(a, b)` | produto vetorial |
| `Length(v)` | comprimento |
| `Normalize(v)` | mesmo sentido, comprimento 1 (vetor zero continua zero) |
| `Distance(a, b)`, `Distance2(a, b)` | distância em 3D e em 2D |
| `Angle(a, b)` | ângulo entre vetores (radianos) |
| `Project(a, b)`, `Reject(a, b)`, `Reflect(v, normal)` | projeção, componente perpendicular, reflexão |
| `LerpVector2`, `LerpVector3(a, b, t)` | interpolação de vetores |
| `Slerp(a, b, t)` | interpolação esférica (direções) |
| `LookAt(origem, alvo)` | direção unitária de `origem` para `alvo` |
| `Flat(v)` | zera o `y` (movimento no chão) |
| `Orthonormal(v)`, `Orthonormal(a, b)` | base ortonormal |
| `ClosestPointOnSegment(p, a, b)` | ponto mais próximo num segmento |
| `Barycentric(p, a, b, c)` | coordenadas baricêntricas |
| `RayPlane(origem, dir, ponto, normal)` | interseção raio–plano |
| `AabbContains(p, min, max)`, `SphereContains(p, centro, raio)` | ponto dentro de caixa / esfera (`bool`) |
| `CubicBezier(t, p0, p1, p2, p3)`, `QuadraticBezier(t, p0, p1, p2)` | curvas de Bézier |
| `Hover(t, base, amplitude)`, `Float(t, base, amplitude)`, `FloatSpin(t, base)` | flutuação de objetos |

```clp
link @clpp.axiom as Axiom;
Vector3 player = Vector3(0, 0, 0);
Vector3 enemy = Vector3(6, 0, 8);
post(Axiom.Distance(player, enemy));
post(Axiom.Normalize(enemy - player));
post(Axiom.Dot(Vector3(1, 0, 0), Vector3(0, 1, 0)));
post(Axiom.Cross(Vector3(1, 0, 0), Vector3(0, 1, 0)));
post(Axiom.SphereContains(enemy, player, 5));
```

```saida
10
(0.6, 0, 0.8)
0
(0, 0, 1)
false
```

### Ângulos

| Função | O que faz |
| --- | --- |
| `Deg(rad)`, `Rad(graus)` | conversão |
| `Sin`, `Cos`, `Tan`, `Asin`, `Acos`, `Atan2(y, x)` | trigonometria (radianos) |
| `NormalizeAngle(rad)` | leva para `(-π, π]` |
| `DeltaAngle(a, b)`, `AngleDiff(a, b)` | menor diferença entre ângulos (radianos) |
| `DeltaAngleDegrees(a, b)` | o mesmo, em graus |

```clp
link @clpp.axiom as Axiom;
post(Axiom.Deg(Axiom.Pi()));
post(Axiom.DeltaAngleDegrees(350, 10));
```

```saida
180
20
```

### Easing

Todas recebem `t` entre 0 e 1 e devolvem o progresso suavizado. Use com `Lerp` para animar.

| Família | Funções |
| --- | --- |
| Sine | `InSine`, `OutSine`, `InOutSine` |
| Quad | `InQuad`, `OutQuad`, `InOutQuad` |
| Cubic | `InCubic`, `OutCubic`, `InOutCubic` |
| Quart | `InQuart`, `OutQuart`, `InOutQuart` |
| Quint | `InQuint`, `OutQuint`, `InOutQuint` |
| Expo | `InExpo`, `OutExpo`, `InOutExpo` |
| Circ | `InCirc`, `OutCirc`, `InOutCirc` |
| Back | `InBack`, `OutBack`, `InOutBack` |
| Elastic | `InElastic`, `OutElastic`, `InOutElastic` |
| Bounce | `InBounce`, `OutBounce`, `InOutBounce` |
| — | `Linear` |

```clp
link @clpp.axiom as Axiom;
for (let frame in 5) {
  let t = frame / 4.0;
  post(Axiom.Lerp(0, 100, Axiom.InOutCubic(t)));
}
```

```saida
0
6.25
50
93.75
100
```

### Aleatoriedade e ruído

| Função | O que faz |
| --- | --- |
| `Random(min, max)`, `RandomRange(min, max)` | número aleatório no intervalo |
| `Gaussian()`, `Gaussian(média, desvio)` | distribuição normal |
| `Weighted(pesos)` | índice sorteado de acordo com os pesos da lista |
| `HashU32(x)` | hash inteiro determinístico |
| `Value1(x)`, `Value2(x, y)`, `Value3(x, y, z)` | ruído de valor (determinístico, entre 0 e 1) |

### Cores

Cores são `Vector3` com componentes entre 0 e 1.

| Função | O que faz |
| --- | --- |
| `FromHSV(h, s, v)` | cor a partir de matiz, saturação e brilho |
| `LerpColor3(a, b, t)` | mistura de cores |
| `LerpHSV(a, b, t)` | mistura passando pelo espaço HSV |
| `Contrast(cor, fator)` | ajusta o contraste |

```clp
link @clpp.axiom as Axiom;
post(Axiom.FromHSV(0, 1, 1));
```

```saida
(1, 0, 0)
```

## Fs: arquivos

| Função | O que faz |
| --- | --- |
| `Fs.read(caminho)` | conteúdo do arquivo como texto (`""` se não existir) |
| `Fs.size(caminho)` | tamanho em bytes (`-1` se não existir) |
| `Fs.list(pasta)` | lista com os nomes dos itens da pasta |

Caminhos com `..` são recusados, para que um script não leia fora da pasta do jogo.

```clp
link @clpp.fs as Fs;
post(Fs.size("arquivo_que_nao_existe.txt"));
```

```saida
-1
```

## Os e Http

| Função | O que faz |
| --- | --- |
| `Os.env(nome)` | valor de uma variável de ambiente (`""` se não existir) |
| `Http.host(url)` | a parte do host de uma URL |

```clp
link @clpp.http as Http;
post(Http.host("https://example.com:8080/jogos"));
```

```saida
example.com
```

Código rodando dentro de `actor(...)` não tem acesso a `Fs`, `Os` nem `Http`.

## Argumentos do programa

`args()` devolve a lista do que foi passado depois do nome do arquivo: `clpp jogo.clp fácil 3` dá `["fácil", "3"]`.
