# Mudanças da revisão 0.9

Esta revisão partiu do rework em C++20 (pasta `CLPP_Rework`) e do estudo do CL++ original em Rust → Luau. A sintaxe foi mantida: `<<`, `let`/`let mut`, `.:`, `post`, `@`, `~>`, `shl`/`shr`, `and`/`or`/`not`. O que mudou foi a correção do que estava errado, o que faltava para a linguagem ser usável de verdade, o editor e a documentação.

Cada item abaixo tem um teste de regressão em `tests/unit/test_regressions.cpp` ou um exemplo verificado na documentação.

## Direção

- **CL++ é uma linguagem própria, não um front-end de Luau.** A VM é a referência de semântica; o exportador de texto Luau continua como opção para um subconjunto. Resquícios específicos do Roblox foram tirados dos exemplos (`link @clpp.roblox`, que era ignorado em silêncio).
- **Arquivos `.clp`.** Todos os exemplos, testes e a extensão usam só `.clp`.
- **Sem cabeçalhos.** Cada arquivo é um módulo; `link` lê o módulo e expõe funções, tipos e constantes.

## Correções de comportamento

| Antes | Agora | Por quê |
| --- | --- | --- |
| Um comentário `<<` em arquivo com fim de linha Windows (CRLF) engolia o resto do arquivo | CRLF, LF e CR terminam a linha | arquivos criados no Windows quebravam sem aviso |
| `7 / 2` dava `3.5` mesmo com dois `int` | `int / int` é divisão inteira (`3`); com um `float` é decimal | coerente com os tipos declarados e com C++/C# |
| `post(true)` imprimia `1` | imprime `true`; em `.:` também | bool é um tipo, não um número |
| `"AZ"[1]` dava `90` (o byte) | dá `"Z"`, contando caracteres UTF-8 | texto é texto |
| Índice fora da lista devolvia lixo ou travava | erro `index out of range: i (size n)`, capturável | segurança |
| `report(x)` só imprimia | interrompe com `x` como erro, capturável com `try`/`catch` | era indistinguível de `post` |
| `warn(x)` ia para a saída normal | vai para a saída de erros com `warning:` | separar log de resultado |
| `a \| b`, `a & b`, `shl`, `shr` com variáveis davam o valor de `a` | resultado correto | bug de alias de registrador na VM |
| `"abc" != "abd"` não compilava | compila | comparação de literais distintos era tratada como erro |
| `facing == Direction.East`, `alive == true`, `p == q` não compilavam | igualdade entre enums, bools, structs, listas e vetores, por valor | básico |
| Função com `-> int` e `return` só dentro de um `if` compilava | `missing return` se algum caminho não devolve | erro silencioso virava `0` |
| `link @clpp.x;` sem `as` era ignorado | usa o último nome com inicial maiúscula (`Axiom`) | importação silenciosamente vazia |
| `parallel(...)` somava os resultados e só aceitava números | devolve a lista de resultados, de qualquer tipo | soma era um caso particular |
| `post(lista)` imprimia `1, 2, 3`; `post(Vector3(...))` imprimia só `x` | `[1, 2, 3]`, `{chave: 1}`, `(3, 4)`, `(1, 2, 3)` | legível e sem ambiguidade |
| Erros de execução sem posição | `arquivo:linha:coluna: runtime error: ...` | localizar o erro |
| Erros em `a + b` apontavam para 1:1 | apontam e sublinham a expressão | editor |
| Uma thread que terminava se juntando a si mesma derrubava o processo ("Resource deadlock avoided") | corrigido | travamento intermitente |
| Mensagens de erro de módulos apontavam para memória liberada | mensagens são donas do texto | encontrado com AddressSanitizer |

## O que passou a existir

| Recurso | Exemplo | Capítulo do guia |
| --- | --- | --- |
| Atribuir campos, campos aninhados, componentes, elementos | `p.stats.hp -= 3; pos.y = 1; xs[i] = v; team[1].hp = 9;` | 7, 9 |
| Métodos que alteram o objeto | `p.damage(5)` altera `p` | 7 |
| `@campo` dentro de métodos = `self.campo` | `@hp = @hp - 1;` | 7 |
| Construtor com argumentos nomeados | `Item(price: 150, name: "espada")` | 7 |
| `continue` | em `while`, `for` e `for … in` | 5 |
| `for … in` sobre listas, textos e dicionários | `for (let item in inventory)` | 5 |
| `len`, `push`, `pop`, `insert`, `remove` | `push(queue, "ana");` | 9 |
| Listas de qualquer tipo; `find` com textos e structs | `list("a", "b")` | 9 |
| Texto: `split`, `joinAll`, `replace`, `startsWith`, `endsWith`, `repeat`, `toNumber`, `indexOf` | `Text.split("a,b", ",")` | 9 |
| Vetores: `-` entre vetores, `/` por número | `enemy - player` | 4 |
| Axiom: `Dot`, `Cross`, `Length`, `Normalize`; predicados devolvem `bool` | `Axiom.Normalize(dir)` | 14 |
| Tipos de módulo qualificados e genéricos em parâmetros | `func f(Combat.Fighter x, array<int> xs)` | 6, 10 |
| Campo do retorno de uma função | `make().hp` | 7 |
| `-> void` | `func log(string m) -> void` | 6 |
| Signals conectando funções de módulo | `damaged ~> Hud.onDamage;` | 13 |
| Módulos que importam módulos, inclusive em losango | A e B importam C | 10 |
| Erros de bibliotecas capturáveis | `try { Text.toNumber("x") } catch (e)` | 11 |

## Editor (VS Code) e servidor de linguagem

O servidor de linguagem foi reescrito sobre nlohmann/json (o anterior gerava JSON inválido na resposta de `initialize`) e segue o LSP 3.17 com posições UTF-16. A extensão ganhou um cliente próprio (o ambiente de build não tinha acesso ao npm para usar `vscode-languageclient`), testado contra uma API `vscode` simulada e o servidor real.

- Diagnósticos ao digitar, inclusive de módulos importados (na linha do `link`).
- Autocompletar: membros herdados com "(herdado de X)", membros de módulos, constantes, retorno de funções (`make().`), `link @clpp.` e caminhos de arquivo, palavras-chave, funções embutidas e modelos de comando.
- Hover em markdown, assinatura com parâmetro ativo, definição entre arquivos, referências, destaque, renomear (inclusive atribuições), contorno, dobras, formatação, tokens semânticos.
- Correção rápida `let` → `let mut`.
- Executar com Ctrl+F5, reinício automático do servidor, barra de status.
- Gramática TextMate refeita: palavras-chave por grupo, templates com `${}`, declarações, receptor `@`, chamadas, tipos.
- Ícone novo (decágono amarelo com a torre preta) no executável, na extensão e nos arquivos `.clp`.
- O `.vsix` traz o `clpp.exe` embutido para Windows x64.

## Desempenho

Detalhes e metodologia no [guia, capítulo 16](guia/16-desempenho.md).

| Programa | Antes (s) | Agora (s) |
| --- | --- | --- |
| `fib(27)` | 1,605 | 0,095 |
| laço de 3 milhões | 0,379 | 0,180 |
| 300 mil chamadas virtuais | 0,595 | 0,109 |
| 200 mil passos de física | 0,353 | 0,050 |
| 100 mil templates | 0,034 | 0,023 |

## Engenharia

- CMake dividido em bibliotecas com dependências explícitas (`clpp_std`, `clpp_front`, `clpp_compiler`, `clpp_vm`, `clpp_tools`).
- Suíte: testes unitários, regressões, exemplos com saída esperada, stress, LSP ponta a ponta, extensão, e **todos os exemplos da documentação** executados pelo CTest. 226 testes.
- Build para Windows a partir do Linux (`cmake/toolchains/llvm-mingw-x86_64.cmake`), com ícone e versão no `clpp.exe`.
- Toda a suíte roda com AddressSanitizer e UndefinedBehaviorSanitizer.

## Limitações conhecidas

- Funções não são valores: lambdas só podem ser chamadas onde são escritas.
- `pcall` devolve `1`/`0`, não o erro.
- O coletor de lixo só gerencia a tabela `@`; structs e listas são valores copiados.
- `Value` ocupa 104 bytes; reduzir isso é o próximo passo de desempenho.
- O núcleo ainda depende da biblioteca padrão para as fontes dos módulos `@clpp.*`.
