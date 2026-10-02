# Arquitetura do compilador CL++

## Pipeline

```text
.clp ─► lexer ─► parser ─► (link: módulos importados, analisados isoladamente e mesclados)
     ─► binder (nomes, slots, campos, vtables) ─► verificador de tipos
     ─► codegen ─► CLIR v1 (bytecode de pilha)
     ─► verify_bytecode + verify_stack ─► lower_registers ─► VM de registradores (+ GC)
```

`analyze_program` produz um `AnalysisResult` (programa, diagnósticos, módulos importados, tokens e os textos de origem que os `string_view` referenciam). O compilador e o editor (LSP) usam a mesma análise; o editor nunca executa o código do usuário. `Compiler::compile` é uma fachada para `analyze_program` + `emit_clir`.

## Bibliotecas

| Biblioteca | Conteúdo | Depende de |
| --- | --- | --- |
| `clpp_std` | funções nativas: texto, arquivos, sistema, Axiom | tipo `Value` |
| `clpp_front` | lexer, parser, binder, verificador de tipos, módulos | `clpp_std` (fontes dos módulos `@clpp.*`) |
| `clpp_compiler` | codegen, CLIR, verificadores, conversão para registradores, exportador Luau | `clpp_front` |
| `clpp_vm` | VM de registradores e GC | `clpp_compiler`, `clpp_std` |
| `clpp_tools` | consultas de IDE, servidor LSP (nlohmann/json), REPL | `clpp_vm`, `clpp_front` |
| `clpp_core` | tudo junto (INTERFACE), para o CLI, os testes e quem embute | — |

Cada biblioteca liga só o que usa, então uma dependência indevida aparece como erro de ligação.

## Decisões

- **Sem cabeçalhos.** Um `link` analisa o módulo importado, confere que ele só declara coisas e mescla as declarações com o nome do módulo como prefixo. Erros do módulo aparecem na linha do `link`.
- **Semântica de valor.** Structs, listas e vetores são copiados ao atribuir. Métodos que alteram `self` devolvem o `self` final ao chamador (`MarkSelf`/`SelfBack`), então `p.damage(5)` altera `p`.
- **Duas formas de bytecode.** A forma de pilha é simples de gerar e de verificar; a de registradores (estilo Lua 5) é rápida de executar. Os verificadores rodam entre as duas.
- **Quadros do tamanho da função**, reaproveitados, em vez de 256 registradores por chamada.
- **A VM é a referência.** O exportador de texto Luau cobre um subconjunto e é opcional ([backend-capabilities.md](backend-capabilities.md)).

## Próximos passos

- Separar o núcleo da biblioteca padrão (registrar nativos por tabela), para embutir o compilador sem `Fs`/`Os`/`Http`.
- `Value` menor e coletor de lixo para todos os objetos.
- Funções como valores de primeira classe.
- Formato binário estável para distribuir bytecode (hoje a CLIR v1 é só em memória; ver [clir-v1.md](clir-v1.md)).

Build, testes e sanitizadores: [toolchain.md](toolchain.md).
