# 16. Desempenho

## Resultados

Mediana de 7 execuções, compilação `release`, Intel Xeon 2,1 GHz (2 núcleos), Linux. "Antes" é o rework no início da revisão; "CPython" é o Python 3.11 rodando o programa equivalente (`benchmarks/*.py`). Tempo em segundos, incluindo compilar e iniciar.

| Programa | O que mede | Antes | Agora | Ganho | CPython 3.11 |
| --- | --- | --- | --- | --- | --- |
| `fib` | ~635 mil chamadas recursivas (`fib(27)`) | 1,605 | 0,095 | 16,9× | 0,031 |
| `loop` | laço aritmético, 3 milhões de voltas | 0,379 | 0,180 | 2,1× | 0,264 |
| `structs` | 300 mil chamadas virtuais com herança | 0,595 | 0,109 | 5,5× | 0,035 |
| `vectors` | 200 mil passos de física com `Vector3` | 0,353 | 0,050 | 7,1× | 0,087 |
| `strings` | 100 mil templates e concatenações | 0,034 | 0,023 | 1,5× | 0,024 |

O CL++ já é mais rápido que o CPython em laços aritméticos, matemática vetorial e textos. Em chamadas de função e de método ainda fica atrás: a seção "Próximos passos" explica por quê e o que falta.

Para reproduzir:

```text
python3 benchmarks/run.py build/release/src/clpp --runs 7 --markdown
python3 benchmarks/run.py build/release/src/clpp --baseline caminho/do/clpp-antigo
```

## O que foi feito

Cada mudança foi medida com perfilador (`perf`) antes e depois; só ficaram as que melhoraram o tempo sem mudar nenhuma saída de teste.

### 1. Quadro de função do tamanho certo, reaproveitado

**Antes:** cada chamada criava um vetor de 256 registradores, mesmo que a função usasse 5. Em `fib`, ~635 mil chamadas significavam mais de 160 milhões de valores construídos e destruídos.

**Agora:** o compilador calcula quantos registradores cada função realmente usa (`function_frame`) e a VM reaproveita quadros de uma reserva (`take_frame` / `give_frame`). Chamar uma função passou a custar pouco mais que copiar os argumentos. Esse é o principal ganho de `fib` e `structs`.

### 2. Caminho rápido para números

**Antes:** atribuir um número a um registrador copiava também o texto e a lista de campos do `Value` (vazios, mas a cópia ainda alocava e liberava). O perfilador mostrava ~50% do tempo de um laço aritmético nisso.

**Agora:** `Value` tem uma atribuição especializada: se o valor é número, só o `double` é copiado (`set_number`). Invariante: um número nunca possui texto nem campos. Esse é o ganho de `loop` e `vectors`.

### 3. Leitura de índice sem cópia

**Antes:** `lista[i]` copiava a lista inteira para ler um elemento.

**Agora:** a VM lê por referência e copia só o elemento.

### 4. Divisão inteira e operações tipadas

Quando os dois lados são `int`, o compilador emite instruções especializadas (`IntAdd`, `IntDiv`), que pulam verificações de tipo na execução.

### 5. VM de registradores

O código passa por duas formas: primeiro uma pilha (fácil de gerar e verificar), depois é convertido para uma máquina de registradores no estilo da VM do Lua 5. A conversão remove cópias redundantes (`fold_copies`) e faz leituras de variáveis irem direto ao registrador da variável. Na revisão foram corrigidos casos em que o "atalho" ficava desatualizado depois de uma operação (por exemplo `a | b` com variáveis), com testes de regressão para cada operador.

## Segurança sem custo de desempenho

- **Verificadores:** antes de rodar, o bytecode passa por um verificador estrutural (saltos, constantes, funções) e um verificador de pilha. Bytecode corrompido é recusado em vez de travar o processo.
- **Limites:** recursão infinita vira o erro `stack overflow` (limite configurável, padrão 200 000 chamadas) em vez de derrubar o jogo.
- **Índices conferidos:** acesso fora de lista ou texto é erro capturável, nunca leitura de memória inválida.
- **Sanitizadores:** toda a suíte roda também com AddressSanitizer e UndefinedBehaviorSanitizer (`cmake --preset asan`). Foi assim que apareceram, e foram corrigidos, um uso de memória após liberar nas mensagens de erro de módulos e um uso de objeto temporário no lexer.
- **Isolamento:** `actor(...)` roda sem acesso a arquivos, rede e ambiente.

## Próximos passos

| Item | Por que importa |
| --- | --- |
| `Value` menor (hoje 104 bytes) | menos memória copiada por chamada; é o que separa `fib` e `structs` do CPython |
| Coletor de lixo para todos os objetos | hoje só a tabela `@` é gerenciada; structs e listas usam cópia de valor |
| Funções como valores | guardar lambdas em variáveis e passá-las como argumento |
| `pcall` devolvendo o erro | hoje devolve só `1` ou `0` |
| Núcleo separado da biblioteca padrão | permitir embutir o compilador sem `Fs`/`Os`/`Http` |
