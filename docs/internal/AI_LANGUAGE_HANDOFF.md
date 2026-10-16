# Handoff para implementar ferramentas em CL++

Atualizado em 2026-10-14. A outra IA pode ler este arquivo sem depender do histórico
da conversa. Nenhuma mensagem foi enviada para outra task. As referências abaixo
são o contrato do C++ VM neste checkout; documentos antigos em `tests/docs/legacy`
servem apenas a regressões históricas.

O CL++ fornece capacidades gerais para CLIs. Protocolos, configuração e builds
específicos de Cluaupp pertencem ao projeto externo. O protótipo inserido aqui
foi retirado por correção de escopo; preserve as APIs e regressões gerais.

## Ordem de leitura obrigatória para gerar código correto

1. [Contrato consolidado](../reference/language-contract.md): todas as famílias de
   sintaxe, semântica de valores/objetos, execução e restrições.
2. [Inventário do lexer](../reference/keyword-inventory.md): cada termo reservado,
   distinguindo formas atuais das removidas.
3. [Gramática](../reference/ebnf.md) e [mapa](../reference/syntax.md): estrutura e
   capítulos detalhados; a EBNF é um resumo, não substitui regras semânticas.
4. [Exemplos completos](../../examples/language/README.md) e `cases.json`: dez
   entradas, módulo importado, saídas integrais e casos inválidos deliberados.
5. [Capacidades aplicadas](../../examples/capabilities/README.md): procedural,
   pipelines genéricos, JSON tipado, conciliação e zelador de chamadas.
6. [Biblioteca padrão](../guide/14-standard-library.md),
   [console/arquivos/JSON](../guide/19-audio-console-automation.md) e fontes de
   `src/stdlib/`: confirme assinaturas, casing, forma de dados e erros reais.
7. [Backlog Cluaupp](CLUAUPP_MIGRATION.md): requisitos e critérios que ainda faltam.

## Regras que não devem ser inventadas

- Funções: `Resultado Nome(Tipo argumento)`. `func` somente para lambda com `-> Resultado`.
- `def` agrupa dados/aliases; `enum class` usa `Tipo::Caso`; receptor é `@this`.
- Defaults em campos, nunca em parâmetros. Argumentos de chamada posicionais.
- Expressão tipada: `(Outcome "ongoing")`, com checagem de compatibilidade;
  não usar `"ongoing" :: Outcome` nem converter strings arbitrárias em aliases.
- Comentários `<<` e `<<[ ... ]>>`; nenhuma migração para comentários C++.
- Arrays/dicionários/objetos têm identidade compartilhada; const congela dados por aliases.
- Conexão comum `::Connect`; gerenciada `~>Connect`; `::Clear` limpa só gerenciadas.
- `Wait` async retorna o payload/tuple; handlers síncronos seguem ordem de inscrição
  e param no primeiro erro. Recursos nativos são limpos ao terminar, após workers.
- Dicionário sem chave lança erro; chamadas protegidas com `:`, destructuring e
  leitura ausente optional são propostas futuras explicitamente adiadas.
- `decltype` atualmente cria binding imutável. `extern` não é FFI geral.
- JSON de um def comum não vira automaticamente objeto com nomes de campos;
  use `Json.Value`/Entries e StringifyValue para o protocolo de configuração.

## Limites para o Cluaupp

O rework C++ executa na VM. Não tem emissor Luau, source maps Luau ou contrato
`api compile`. O Cluaupp TypeScript está em `../Cluaupp` e seu adaptador legado
exige compilador >=0.8.0 e <0.9.0, contrato 1, manifest e tags específicos.
Um binário VM 0.10 não satisfaz esse contrato. Não simule compatibilidade por
comparação frouxa de versão e não confunda o produto Rust/Luau com este VM.

APIs host de Process/Path e Io.Error estão implementadas neste checkout,
em `src/stdlib/std_process.cpp` e demais fontes modificadas. Leia o
[contrato de host CLI](../reference/cli-host.md). Elas ainda precisam
de validação Linux/macOS e integração/distribuição. O host Cluaupp não está
portado por inteiro: build de projeto, cache, Rojo, watch, daemon e distribuição
permanecem pendentes. Consulte o backlog antes de afirmar que existem.

## Como conferir e como ampliar

Execute `tests/examples/check_language_contract.py <compilador>` para comparar
saídas, rejeições e o inventário de keywords. Execute também o teste de todos os
sources e o de snippets da documentação. Um novo keyword precisa de explicação
no inventário; uma nova entrada deste corpus precisa de saída em cases.json.
Uma feature composta precisa de casos positivos e negativos próprios: a tabela
não prova ausência de bugs, nem substitui parser/checker/VM tests.

O site é gerado de guide/handbook/reference com `npm --prefix www run build`.
Handoff interno não é publicado. Preservar os arquivos que o autor está editando,
especialmente `examples/projects/combat/main.clp`. A instrução mais recente do
autor autoriza publicação no GitHub após validação; não marque plataformas,
pacotes ou efeitos como aprovados antes de executá-los.

## Evidência local mais recente

A expressão `(Outcome "ongoing")` está implementada, inclusive para aliases
importados e contextos de callbacks/arrays. A rodada Windows normal passou em
523 checks; 458 casos unitários passaram sob ASan/UBSan. O corpus dedicado tem
26 checks e a documentação executa 213 exemplos; 19 propostas permanecem
excluídas. Os 21 testes históricos do protótipo não representam suporte específico da
linguagem. O protótipo foi retirado; os testes de arquivos/processos são gerais. Foram corrigidos try handlers após
return/break/continue e caminhos de arquivos Unicode/NUL no Windows.
VS Code 0.10.4 e Zed receberam o compilador normal desta fonte.
Recarregar os editores aplica a atualização. Isso não comprova Linux/macOS/MSVC,
backend Luau, self-hosting ou publicação remota. Consulte .qa/REPORT.md para
os limites e a distinção entre testes de correção e latência.
