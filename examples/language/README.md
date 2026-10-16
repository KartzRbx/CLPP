# Exemplos do contrato CL++

Para a outra IA: leia primeiro o [contrato da linguagem](../../docs/reference/language-contract.md),
o [inventário dos termos reservados](../../docs/reference/keyword-inventory.md) e
o [handoff de implementação](../../docs/internal/AI_LANGUAGE_HANDOFF.md).
O contrato está em inglês para integrar o site e descreve a sintaxe atual,
semântica, limites, formas removidas e propostas futuras. Não deduza APIs por
semelhança com TypeScript, C++ ou Luau.

Cada programa abaixo é uma entrada completa e independente. O arquivo
`cases.json` registra a saída integral e também programas que devem ser rejeitados.
Os módulos de `lib/` são importados pelas entradas; não são programas de demonstração
isolados. Os comentários usam `<<` e `<<[ ... ]>>`; métodos usam `@this`.

| Arquivo | Cobertura |
| --- | --- |
| `01_values.clp` | Comentários, pragma, literais, exatidão de number, float, const/constexpr, auto, decltype e interpolação |
| `02_operators.clp` | Aritmética, atribuição composta, incrementos, lógica, bits, comparações, cast e ternário |
| `03_flow.clp` | if/else, while/do/for, break/continue, switch, match/guard, optional e erros |
| `04_collections.clp` | Arrays, dicionários, operações de mutação, ranges/slices, Unicode e vetores |
| `05_objects.clp` | def, defaults, private/public, contratos abstratos, herança, override/final, @this, método externo e callback ligado |
| `06_types_callbacks.clp` | Aliases, unions, enums, generic data/functions/methods, bounds/where, especializações e capturas |
| `07_events_async.clp` | signal, Once, janitor interno, Clear, Wait, tuple, observable, async callbacks e parallel |
| `08_workers.clp` | thread/join, atomic, mutex, actor e coroutine |
| `09_advanced_forms.clp` | extern restrito, operator+/- de dados, move local e variádico dinâmico existente |
| `10_modules.clp` | link relativo/nativo, export, tipos qualificados e inicialização compartilhada |

Execute da raiz do repositório, ajustando o caminho do compilador ao seu build:

```powershell
python tests/examples/check_language_contract.py build/windows-sanitizers/src/clpp-vm.exe --junit build/language-contract.xml
python tests/examples/check_sources.py build/windows-sanitizers/src/clpp-vm.exe
python tests/docs/check_docs.py build/windows-sanitizers/src/clpp-vm.exe
```

Um build com sanitizers precisa das DLLs do toolchain no PATH. O teste não instala
ferramentas, não abre janelas e não depende de serviços externos. Os exemplos GUI,
buffers, bibliotecas e aplicações completas estão no restante de `examples/`;
consulte seu README e os testes de saída correspondentes.

O corpus não demonstra um backend Luau, watch, Rojo ou self-hosting. Essas frentes
continuam no backlog do Cluaupp e precisam de contratos/testes próprios.
