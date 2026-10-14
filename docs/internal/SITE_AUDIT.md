# Revisão integral do site CL++

## Fonte de verdade e decisões do autor

O site ensina a linguagem alvo definida pelo autor. O código de combate escrito pelo autor e o manual de quinze capítulos do ZIP orientam o conteúdo, mas nomes de funções auxiliares e afirmações de implementação nos anexos não são provas de suporte atual.

Decisões confirmadas nesta conversa:

- sem headers e sem extensão de exportação por header;
- declarações com tipo ou auto, mutáveis por padrão; const para imutabilidade;
- sem let ou mut na linguagem pública;
- def reúne formas de dados, aliases, uniões, tipos de função, genéricos e herança;
- inicializadores nomeados usam Campo = valor, sem ponto; acesso usa objeto.Campo;
- saída por post, warn e report; log não é uma função intrínseca;
- primeiro revisar o site, depois atualizar o compilador;
- a sintaxe do autor é principal, com estado claro dos recursos que faltam no compilador.

## Verificação ramificada

Três frentes revisaram todas as páginas existentes, cruzando linguagem e fontes. Depois cada frente reviu um terço dos novos capítulos em português. A revisão cruzada encontrou e corrigiu construção posicional residual, receiver não definido, semântica de cópia aplicada a objetos alvo, recomendações de Result não especificado, comentários antigos, campos nomeados com ponto, claims de instalador/Rust e propriedades de Json.Set.

## Divergências do compilador atual

O checkout usa C++20, bytecode e VM. O parser aceita várias formas antigas que não são a linguagem pública definida pelo autor. Permanecem pendentes: def; constantes tipadas; funções nomeadas por retorno; callbacks tipados armazenáveis; enum class e casos com ::; match com =>; for tipado; optional e narrowing/guard; comentários // e /* */; pragma; referência compartilhada de objetos; casts e contratos de módulos/contextos.

A fonte atual também restringe extern ao caminho strlen, mantém bindings inferidos de coleção imutáveis, imprime pcall como 0/1 e usa uma precedência unária incomum. Os nomes/códigos de diagnóstico fornecidos são contratos a implementar, não códigos aprovados no compilador C++20.

O manual proposto descreve integração Luau/Roblox. Ela é explicada como objetivo de integração, sem afirmar que o executável atual ou um instalador antigo já produz esse backend. Capturas, segurança de contexto, janitor, formatter e migração automática precisam de testes futuros.

## Cobertura

Todas as páginas de guide/ e reference/ são publicadas automaticamente. Os quinze capítulos do manual corrigido e traduzido são publicados em handbook/, com navegação e conteúdo integralmente em inglês. O índice explica a linguagem antes das APIs. Cada bloco alvo recebe aviso visível e link para o estado do compilador.

O validador de conteúdo cobre páginas de guide/, reference/, handbook/, homepage e README. Rejeita declarações antigas, log intrínseco, headers em código, inicializadores nomeados com ponto, e sintaxe pendente sem marcador. Confere links Markdown e os caminhos/arquivos no HTML gerado. A checagem de exemplos executáveis contabiliza exemplos alvo separadamente; não os marca como aprovados.

## Limites desta etapa

Não houve atualização do compilador, editor ou backend. Não houve publicação nem push. Exemplos alvo não foram executados como se fossem implementados. A revisão visual automatizada no navegador permanece indisponível por falha de ACL do ambiente; a estrutura, mensagens e links do HTML gerado são verificados programaticamente.

## Correções posteriores do autor

Todo o site deve ser publicado em inglês, incluindo o manual de quinze capítulos, os comentários explicativos nos exemplos, a homepage, os títulos e a navegação. Os slugs históricos foram preservados para não quebrar os links existentes.

A VM própria é o único destino oficial do CL++. Luau/Roblox é um caso de uso para um transpilador implementado em CL++, não um backend oficial da linguagem. As explicações publicadas foram corrigidas para refletir essa decisão.

## Evidências

As saídas finais dos validadores e do build ficam em [site-validation.txt](./site-validation.txt), nesta pasta. Resultado: 51 fontes verificadas, 207 blocos CL++ (164 especificações alvo), 243 links Markdown locais; 36 exemplos executáveis aprovados, zero falhas; 51 páginas geradas, 50 páginas indexadas na busca; todos os links e assets /CLPP/ do HTML existem. Os 163 blocos alvo encontrados nos documentos pelo teste executável foram excluídos explicitamente, sem contar como suporte do compilador. O README contém o bloco alvo adicional. O HTML confirma os quinze capítulos e os inicializadores sem ponto, preservando acesso a membros.

## Inventário de páginas

- `docs/guide/00-about.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/00-language-tour.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/01-getting-started.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/02-lexical-syntax.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/03-variables-and-types.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/04-operators.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/05-control-flow.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/06-functions.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/07-structs-and-inheritance.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/08-enums-variants-option-result.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/09-collections-and-text.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/10-modules.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/11-errors.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/12-concurrency.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/13-events.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/14-standard-library.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/15-tools-and-editor.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/16-performance.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/17-window-and-graphics.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/18-user-interfaces.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/19-audio-console-automation.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/20-modular-combat.md`: revisada; sintaxe e status de implementação separados.
- `docs/guide/21-account-screen.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/comments.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/def.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/ebnf.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/identifiers.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/keywords.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/lexer.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/literals.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/punctuation.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/status.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/syntax.md`: revisada; sintaxe e status de implementação separados.
- `docs/reference/toolchain.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/00-o-que-e-o-clpp.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/01-comecando.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/02-modelo-mental.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/03-tipos-e-variaveis.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/04-operadores.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/05-controle-de-fluxo.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/06-funcoes.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/07-def-e-tipos.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/08-colecoes.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/09-modulos.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/10-contextos-e-seguranca.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/11-eventos-e-concorrencia.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/12-erros-e-ferramentas.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/13-roblox-e-cluaupp.md`: revisada; sintaxe e status de implementação separados.
- `docs/handbook/14-referencia.md`: revisada; sintaxe e status de implementação separados.

Validação após tradução: todo o HTML passou na checagem de idioma; os quinze capítulos usam lang=en. O cache antigo do Astro foi removido e o build limpo gerou conteúdo traduzido sem os avisos de IDs duplicados. Os 36 exemplos executáveis passaram novamente; 163 blocos de especificação permaneceram separados.
