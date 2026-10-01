# 15. Ferramentas e editor

## Linha de comando

| Comando | O que faz |
| --- | --- |
| `clpp arquivo.clp [args...]` | compila e executa; `args()` recebe os argumentos extras |
| `clpp --repl` | modo interativo |
| `clpp --lsp` | servidor de linguagem (LSP 3.17) para editores |
| `clpp --version` | versão |
| `clpp --help` | ajuda |

Código de saída: `0` quando o programa termina bem, diferente de zero em erro de compilação ou de execução. Mensagens de erro vão para a saída de erros no formato `arquivo:linha:coluna: mensagem`.

## Extensão do VS Code

A extensão (`clpp-language-<versão>.vsix`) traz o `clpp.exe` embutido para Windows x64, então funciona logo depois de instalada. Para instalar: **Extensões → ⋯ → Instalar do VSIX…**.

### O que ela faz

| Recurso | Como usar |
| --- | --- |
| Realce de sintaxe | automático em arquivos `.clp` |
| Erros enquanto digita | sublinhado vermelho e painel **Problemas**; erros de módulos importados aparecem na linha do `link` |
| Autocompletar | variáveis, funções, campos, métodos (inclusive herdados), membros de módulos, constantes, palavras-chave, funções embutidas, modelos de comando |
| Autocompletar de `link` | `link @clpp.` lista a biblioteca padrão; `link "./` lista arquivos `.clp` e pastas |
| Assinatura | ao digitar `(` mostra os parâmetros e destaca o atual |
| Informação ao passar o mouse | tipo, assinatura, de qual módulo vem, membros de structs |
| Ir para definição | F12, inclusive para dentro de outros arquivos |
| Referências e destaque | Shift+F12; o nome sob o cursor é destacado no arquivo |
| Renomear | F2, atualiza declaração, usos e atribuições |
| Correção rápida | `let` → `let mut` quando há atribuição a uma variável imutável |
| Estrutura do arquivo | contorno (Outline) com structs, métodos, funções e enums |
| Dobrar código | blocos `{ }` e comentários de bloco |
| Formatar documento | Shift+Alt+F |
| Tokens semânticos | cores por papel (parâmetro, campo, função, tipo) |
| Executar | Ctrl+F5 ou o botão ▶ no canto do editor |

### Comandos

| Comando | Atalho |
| --- | --- |
| CL++: Executar arquivo | Ctrl+F5 |
| CL++: Reiniciar servidor de linguagem | — |
| CL++: Mostrar saída do servidor | — |

### Configuração

| Opção | Padrão | Efeito |
| --- | --- | --- |
| `clpp.serverPath` | vazio | caminho do `clpp`. Vazio: o embutido na extensão, depois o `PATH`, depois `%LOCALAPPDATA%\Programs\CLPP\clpp.exe` (ou `~/.local/bin/clpp`) |

Se o servidor cair, a extensão o reinicia sozinha (até 5 vezes em 3 minutos) e mostra o estado na barra inferior.

### Como a extensão funciona

O próprio compilador é o servidor de linguagem (`clpp --lsp`): o editor e o compilador usam o mesmo analisador, então o que o editor aceita é exatamente o que compila. Cada edição reanalisa o arquivo e os módulos que ele importa, preferindo a versão aberta (ainda não salva) de cada arquivo. Posições seguem o padrão LSP em UTF-16, então acentos e emojis não deslocam sublinhados.

## Compilar o CL++

```text
cmake --preset release
cmake --build --preset release
ctest --preset debug
```

Presets disponíveis em `CMakePresets.json`: `debug`, `release`, `asan` (AddressSanitizer e UndefinedBehaviorSanitizer), `tidy` (clang-tidy) e `ucrt64` (MSYS2 no Windows). Para gerar `clpp.exe` a partir do Linux:

```text
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-x86_64.cmake -DLLVM_MINGW=/caminho/do/llvm-mingw
cmake --build build-win --target clpp
```

Para empacotar a extensão com o executável:

```text
python3 tools/vscode/package_vsix.py --exe build-win/src/clpp.exe
```

## Testes

`ctest` roda:

| Grupo | O que verifica |
| --- | --- |
| testes unitários (Catch2) | lexer, parser, verificador de tipos, VM, IDE, regressões |
| exemplos | cada `examples/*.clp` produz a saída em `tests/examples/*.out` |
| stress | milhares de programas gerados, concorrência, fuzzing do autocompletar |
| LSP ponta a ponta | conversa JSON-RPC real com `clpp --lsp`, como o VS Code faz |
| extensão | a extensão ativada contra uma API `vscode` simulada e o servidor real |
| documentação | todo exemplo ```` ```clp ```` destes capítulos compila, roda e imprime o bloco `saida` |

## Embutir em um jogo

O núcleo é uma biblioteca C++20 (`clpp_core`). O jogo compila o código-fonte e roda a VM:

```text
#include "clpp/compiler.hpp"
#include "clpp/vm.hpp"

clpp::CompileResult result = clpp::Compiler{}.compile(source, module_loader);
if (!result.ok()) { /* mostrar result.diagnostics */ }
clpp::VirtualMachine vm;
vm.set_output(game_console);
vm.load(result.chunk);
if (!vm.run()) { /* vm.error(), vm.error_location() */ }
```

Funções do jogo entram como `extern func` (capítulo 6). O `module_loader` decide de onde vêm os arquivos de `link`, então o jogo pode servir módulos de dentro de um pacote de recursos.
