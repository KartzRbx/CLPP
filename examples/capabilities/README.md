# CL++: exemplos de capacidade

Oito programas executáveis com resultados determinísticos. Execute a partir da
raiz com `clpp examples/capabilities/01_procedural.clp`, escolhendo o script desejado.

| Script | O que verifica |
| --- | --- |
| `01_procedural.clp` | Funções, loops, recuperação de erro e inteiros acima de 2^53 |
| `02_generic_pipeline.clp` | Map/Filter reutilizáveis, captura de tipo genérico e inferência contextual |
| `03_anti_cheat.clp` | Auditoria procedural de sequência, tempo e deslocamento |
| `04_prefix_queries.clp` | Somas prefixadas comparadas com uma implementação de referência |
| `05_json_config.clp` | Arquivo, ambiente do OS, JSON recursivo tipado e validação de orçamento |
| `06_call_janitor.clp` | Zelador de callbacks LIFO, desconexão, isolamento de falhas e limpeza idempotente |
| `07_async_events.clp` | Payload heterogêneo, Wait assíncrono, eventos, parallel e threads com join |
| `08_reconciliation.clp` | Conciliação com registros de dados, centavos inteiros e invariante contábil |

`lib/collections.clp` e `lib/calls.clp` são módulos reutilizáveis. Os programas são
procedurais; `Entry` é um registro sem métodos. O zelador é código da aplicação,
com `Defer` e `Clean`, e não altera o zelador interno de `evento~>Connect`.
Cada callback sai da fila antes de executar; falhas não impedem os demais e são
contadas. Ele não oferece sincronização entre threads. Callbacks devem terminar
e não cadastrar indefinidamente novas limpezas.

O anti-cheat gera alertas de plausibilidade em dados simulados. Produção exige
estado autoritativo, tolerâncias de latência e tratamento de teleportes permitidos;
esses alertas não justificam automaticamente banimento. Não há inspeção de processos.

A otimização é algorítmica: quatro consultas passam de 22 adições de varredura
para 8 adições de preparação e 4 subtrações, mantendo resultados idênticos.
Isso não mede ganho de tempo do compilador. O JSON mantém categorias distintas
e inteiros de 64 bits; o programa lê apenas `config.json` e `Os.env("PATH")`.
O ledger verifica partidas balanceadas, sem persistência ou transações de banco.

## Validação

Cada programa tem saída esperada em `tests/examples/capabilities/`. Depois de
configurar e compilar o projeto:

```powershell
ctest --test-dir build/ucrt64 -L capabilities --output-on-failure
```

No Windows/UCRT64, inclua `C:\msys64\ucrt64\bin` no PATH para resolver as DLLs.
Os testes rodam da raiz, com limite de 30 segundos por programa, e comparam a
saída completa. Os scripts não precisam de rede, janelas ou serviços externos.
