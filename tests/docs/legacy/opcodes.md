# Bytecode: opcodes da CLIR v1

O compilador gera primeiro um bytecode de pilha (CLIR v1, em `BytecodeChunk`), que é verificado e depois convertido para as instruções de registradores da VM (`RegOp`, 32 bits: `op | A | B | C` ou `op | A | D16`). Esta tabela descreve a forma de pilha. Operandos `u16` são little-endian. O código-fonte de referência é `src/core/compiler/opcode.hpp`.

Notação do efeito na pilha: `[a, b] → [c]` consome `a` e `b` (b no topo) e empilha `c`.

## Valores e variáveis

| # | Opcode | Operando | Efeito |
| --- | --- | --- | --- |
| 0 | `Nop` | — | nada |
| 1 | `Halt` | — | encerra |
| 2 | `Const` | `u16` constante | `[] → [constants[i]]` |
| 10 | `LoadLocal` | `u16` slot | `[] → [local]` |
| 11 | `StoreLocal` | `u16` slot | `[v] → []`, grava no local |
| 43 | `ClearLocal` | `u16` slot | esvazia o local (usado por `move`) |
| 21 | `Dup` | — | `[a] → [a, a]` |
| 84 | `Over` | — | `[a, b] → [a, b, a]` |
| 22 | `Pop` | — | `[a] → []` |

## Aritmética, comparação e bits

| # | Opcode | Efeito |
| --- | --- | --- |
| 5–9 | `Add` `Sub` `Mul` `Div` `Mod` | `[a, b] → [a op b]`; vetores: `+`, `-` entre vetores, `*` e `/` por número |
| 56 | `IntAdd` | soma de dois `int` sem verificação de tipo |
| 76 | `IntDiv` | divisão de dois `int`, truncada em direção a zero |
| 3 | `Concat` | `[a, b] → [texto(a) + texto(b)]`, qualquer valor |
| 14 | `Not` | `[a] → [!a]` |
| 15–20 | `Eq` `NotEq` `Less` `LessEq` `Greater` `GreaterEq` | `[a, b] → [bool]`; igualdade compara textos, structs, listas e vetores por valor |
| 66–71 | `BitAnd` `BitOr` `BitXor` `BitNot` `Shl` `Shr` | operações sobre a parte inteira |
| 72 | `Range` | `[a, b] → [intervalo]` (para fatias) |
| 73 | `Slice` | `[x, a, b] → [x[a .. b]]` |

## Controle

| # | Opcode | Operando | Efeito |
| --- | --- | --- | --- |
| 12 | `Jump` | `u16` destino | salta |
| 13 | `JumpIfFalse` | `u16` destino | `[c] → []`, salta se `c` for falso |
| 23 | `Call` | `u16` função | chama; os argumentos estão na pilha |
| 57 | `VCall` | `u16` slot da vtable, `u16` argumentos | chamada virtual pelo tipo dinâmico do receptor |
| 24 | `Return` | — | `[v] → ` volta ao chamador com `v` |
| 85 | `MarkSelf` | — | num método que altera `self`: guarda o `self` final para quem chamou |
| 86 | `SelfBack` | `u16` slot | depois de `VCall` num local: grava o `self` alterado de volta no local |
| 40 | `Protect` | `u16` destino | início de `try`: em erro, salta para o destino |
| 41 | `EndTry` | — | fim do `try` |
| 74 | `PushError` | — | empilha a mensagem do erro capturado |

## Saída e erros

| # | Opcode | Efeito |
| --- | --- | --- |
| 4 | `Post` | `[v] → []`, escreve `v` e nova linha na saída |
| 77 | `Warn` | `[v] → []`, escreve `warning: v` na saída de erros |
| 78 | `Report` | `[v] → `, levanta um erro com `v` como mensagem |

## Structs, listas, dicionários, vetores

| # | Opcode | Operando | Efeito |
| --- | --- | --- | --- |
| 30 | `MakeStruct` | `u16` n | `[f1..fn] → [struct]` (também listas e dicionários) |
| 58 | `Tag` | `u16` tipo | marca o struct com o tipo (`65535` dicionário, `65534` variant) |
| 29 | `GetField` | `u8` campo | `[s] → [s.campo]`; em vetores, componente |
| 82 | `SetField` | `u8` campo | `[s, v] → [s com campo = v]` |
| 42 | `GetIndex` | — | `[x, i] → [x[i]]`; listas, textos (caractere UTF-8), dicionários, vetores |
| 83 | `SetIndex` | — | `[x, i, v] → [x com x[i] = v]`; dicionário acrescenta chave nova |
| 79 | `ListLen` | — | `[x] → [len(x)]` |
| 80 | `IterLen` | — | `for … in`: número fica igual, coleção dá o tamanho |
| 81 | `IterAt` | — | `for … in`: `[fonte, i] → [item]` |
| 52 | `ListSort` | — | `[lista] → [lista ordenada]` |
| 53 | `ListFind` | — | `[lista, v] → [posição ou -1]` |
| 87 | `ListPush` | `u16` slot | `[v] → []`, acrescenta ao local |
| 88 | `ListPop` | `u16` slot | `[] → [último]`, removido do local |
| 89 | `ListInsert` | `u16` slot | `[i, v] → []` |
| 90 | `ListRemove` | `u16` slot | `[i ou chave] → [removido]` |
| 25 | `MakeVector` | `u8` n (2–4) | `[c1..cn] → [VectorN]` |
| 26–28 | `MakeBuffer` `BufferWrite` `BufferSize` | — | buffers de bytes |
| 31–32 | `GetSelf` `SetSelf` | `u16` nome | tabela `@` fora de métodos |

## Concorrência

| # | Opcode | Efeito |
| --- | --- | --- |
| 33–36 | `MakeTask` `Await` `Spawn` `Parallel` | tarefas `async`; `Parallel` devolve a lista de resultados |
| 65 | `Schedule` | inicia uma chamada `async` |
| 44–45 | `SpawnThread` `Join` | threads nativas |
| 46–48 | `AtomicNew` `FetchAdd` `AtomicLoad` | contadores atômicos |
| 49–51 | `MutexNew` `Lock` `Unlock` | travas |
| 59–62 | `CoCreate` `CoResume` `CoYield` `Defer` | corrotinas e `task.defer` |
| 63 | `Actor` | chamada isolada (sem acesso ao sistema) |

## Nativos

| # | Opcode | Operando | Efeito |
| --- | --- | --- | --- |
| 54 | `CCall` | `u16` id | função `extern` fornecida pelo programa hospedeiro |
| 64 | `Axiom` | `u16` (aridade << 8 \| id) | função da biblioteca Axiom |
| 75 | `Std` | `u16` (aridade << 8 \| id) | função nativa da biblioteca padrão (texto, arquivos, args) |
| 37–39 | `FileSize` `Env` `HttpHost` | — | acesso ao sistema (recusado dentro de `actor`) |

O opcode 55 está reservado e é rejeitado pelo verificador.

## Verificação

`verify_bytecode` confere versão, tamanho (até 65 535 bytes), destinos de salto no início de instruções, opcodes conhecidos e referências a constantes, funções, locais, vtables e tags. `verify_stack` confere a profundidade da pilha em todos os caminhos (inclusive junções após `if`) e recusa underflow. Só depois disso o código é convertido para registradores. Chunks levam `kBytecodeFormatVersion` (1); o formato está descrito em [clir-v1.md](clir-v1.md).
