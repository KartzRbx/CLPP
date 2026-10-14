# 12. Concorrência

Jogos fazem muitas coisas "ao mesmo tempo": carregar recursos, rodar IA, animar, tocar som. CL++ oferece quatro ferramentas, da mais simples para a mais baixa:

| Ferramenta | Para quê | Roda em |
| --- | --- | --- |
| `async` / `spawn` / `await` / `parallel` | tarefas que devolvem um resultado | threads de trabalho |
| corrotinas (`coroutine.*`) e `task.defer` | lógica que pausa e continua (cutscenes, sequências) | a mesma thread |
| `thread` / `join` | trabalho pesado em outra thread | uma thread nativa |
| `atomic`, `mutex`, `actor` | dados compartilhados entre threads | — |

## Tarefas: `async`, `spawn`, `await`

Uma função `async` pode ser iniciada com `spawn`, que devolve uma `task` imediatamente. `await` espera a tarefa terminar e entrega o resultado.

```clp
async string loadAsset(string name) {
  return "carregado: " .: name;
}

task<string> map = spawn loadAsset("mapa");
task<string> music = spawn loadAsset("música");
post(await map);
post(await music);
```

```saida
carregado: mapa
carregado: música
```

`await` direto numa chamada `async` inicia e espera de uma vez:

```clp
async number twice(number x) { return x * 2; }
post(await twice(21));
```

```saida
42
```

## `parallel`: várias tarefas de uma vez

`parallel(...)` inicia todas as chamadas `async`, espera todas e devolve a lista de resultados, na ordem.

```clp
async number cost(number units) { return units * 15; }
const auto results = parallel(cost(2), cost(5), cost(1));
post(results);
auto total = 0;
for (auto r in results) { total += r; }
post(total);
```

```saida
[30, 75, 15]
120
```

## Threads nativas

`thread f(args)` roda a função numa thread do sistema operacional. `join(t)` espera e devolve o resultado.

```clp
number simulate(number steps) {
  auto x = 0;
  for (auto i in steps) { x += i; }
  return x;
}
const auto a = thread simulate(1000);
const auto b = thread simulate(10);
post(join(a) + join(b));
```

```saida
499545
```

Cada thread tem a própria memória: os argumentos são copiados para ela. Para compartilhar um contador entre threads, use `atomic`.

## `atomic` e `mutex`

```clp
atomic hits = 0;
const auto gate = mutex();

number record(number counter, number lockId) {
  lock(lockId);
  fetch_add(counter, 1);
  unlock(lockId);
  return 1;
}

const auto t1 = thread record(hits, gate);
const auto t2 = thread record(hits, gate);
join(t1);
join(t2);
post(atomic_load(hits));
```

```saida
2
```

| Função | Efeito |
| --- | --- |
| `atomic nome = valor;` | declara um contador compartilhável entre threads |
| `fetch_add(a, n)` | soma `n` atomicamente, devolve o valor anterior |
| `atomic_load(a)` | lê o valor atual |
| `mutex()` | cria uma trava |
| `lock(m)` / `unlock(m)` | entra e sai da região protegida |

## `actor`: isolamento

`actor(f, valor)` roda `f` isolada do resto do programa (sem acesso a arquivos, rede ou estado global) e devolve o resultado. Útil para executar lógica de terceiros, como mods.

```clp
number score(number kills) { return kills * 100; }
post(actor(score, 7));
```

```saida
700
```

## Corrotinas

Uma corrotina é uma função que pode pausar (`coroutine.yield(valor)`) e ser retomada depois (`coroutine.resume(co)`). Tudo acontece na mesma thread, sem concorrência real, o que a torna segura para lógica de jogo passo a passo.

```clp
string cutscene() {
  coroutine.yield("câmera se aproxima");
  coroutine.yield("o herói fala");
  return "fim";
}
const auto scene = coroutine.create(cutscene);
post(coroutine.resume(scene));
post(coroutine.resume(scene));
post(coroutine.resume(scene));
```

```saida
câmera se aproxima
o herói fala
fim
```

## `task.defer`: rodar depois

`task.defer(f)` agenda `f` para rodar quando o programa principal terminar o que está fazendo.

```clp
void cleanup() { post("limpeza"); }
task.defer(cleanup);
post("quadro atual");
```

```saida
quadro atual
limpeza
```

## Qual escolher

- Carregar ou calcular algo e usar o resultado depois: `spawn` + `await`.
- Várias coisas independentes: `parallel`.
- Sequência de eventos ao longo do tempo: corrotinas.
- Cálculo pesado e longo: `thread` + `join`.
- Contadores compartilhados: `atomic`; regiões críticas: `mutex`.
