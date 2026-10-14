# Exemplos CL++

Todos os 41 arquivos `.clp` desta pasta e suas subpastas usam a sintaxe atual.
O compilador verifica cada fonte, incluindo os módulos reutilizáveis. As entradas
executáveis têm testes de saída completa; módulos são exercitados pelos programas
que os importam. Execute os comandos a partir da raiz do repositório.

```powershell
clpp examples/instance.clp
clpp examples/projects/combat/main.clp
clpp examples/projects/pacman/main.clp
```

As funções nomeadas usam `Tipo Nome(Tipo argumento)`, definições usam `def`, enums
usam `enum class` e `Tipo::Membro`, e o receptor de método é `@this`. `func` aparece
somente em lambdas. Eventos usam `::Connect` / `~>Connect`, `::Fire` e `::Clear`.
Argumentos são posicionais; valores padrão pertencem a campos de definições.

| Exemplos | Assunto |
| --- | --- |
| `hello`, `arith`, `sum`, `let`, `comments`, `abs` | Fundamentos, constantes, loops e biblioteca |
| `func`, `fibonacci`, `math`, `link` | Funções tipadas, recursão e módulos com export |
| `point`, `instance`, `automation` | Dados, enums, métodos, herança, interfaces, acesso privado e generics |
| `syntax`, `match`, `template` | Panorama da linguagem, match com guard e interpolação de texto |
| `async`, `spawn`, `parallel` | Tarefas tipadas e execução cooperativa |
| `fs`, `vectors_buffers` | Arquivos, vetores e buffers nativos |
| `gfx`, `gui`, `anime_ui`, `ui/` | Desenho e interfaces; requerem as bibliotecas nativas |
| `capabilities/` | Aplicações procedurais, JSON/OS, anti-cheat demonstrativo, otimizações e zelador |
| `projects/combat/`, `projects/pacman/` | Cenários completos e determinísticos na VM |

`instance.clp` substitui o antigo nome `self.clp`. Os nomes históricos `func.clp`,
`let.clp` e `template.clp` foram mantidos, mas seus conteúdos usam funções tipadas,
`const` e interpolação, respectivamente. A API GUI ainda expõe árvores de nós como
`any`; esses exemplos refletem o contrato nativo atual, sem inventar um tipo novo.

Os demos GUI encerram após três frames no modo headless. `gfx.clp` grava
`showcase.png` na pasta de execução. Para usar uma janela, execute sem definir
`CLPP_HEADLESS`. Testes headless verificam execução/saída, não aparência gráfica.

## Verificação

```powershell
python tests/examples/check_sources.py build/ucrt64/src/clpp.exe
ctest --test-dir build/ucrt64 -R "^example |^author |^capability |^all example sources" --output-on-failure
```

No build Windows/UCRT64, inclua `C:\msys64\ucrt64\bin` no PATH para carregar as DLLs.
Os testes de compilação rejeitam construções antigas para evitar que voltem a
aparecer nos exemplos. Os cenários Combat/Pacman não implementam os contratos
externos de Roblox, persistência ou análise de pureza.
