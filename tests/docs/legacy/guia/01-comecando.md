# 1. Começando

CL++ é uma linguagem de programação compilada, de uso geral e pensada para jogos. O código-fonte (`.clp`) é compilado para bytecode e executado por uma máquina virtual própria, com matemática vetorial, corrotinas e tipos de valor no núcleo. Não há arquivos de cabeçalho: cada `.clp` já é o seu próprio módulo.

## Instalar

### Windows

1. Baixe `clpp.exe` e `clpp-language-<versão>.vsix` da página de releases do repositório.
2. Coloque `clpp.exe` em `%LOCALAPPDATA%\Programs\CLPP\` (ou em qualquer pasta do `PATH`).
3. No VS Code: **Extensões → ⋯ → Instalar do VSIX…** e escolha o `.vsix`. A extensão já traz o `clpp.exe` embutido, então funciona mesmo sem o passo 2.

### Compilar a partir do código

Requisitos: CMake 3.20+, Ninja e um compilador C++20 (GCC 13+, Clang 16+ ou MSVC 19.30+).

```text
cmake --preset release
cmake --build --preset release
ctest --preset debug          # opcional: roda toda a suíte de testes
```

No Windows com MSYS2 use o preset `ucrt64`. Para gerar um `clpp.exe` para Windows a partir do Linux existe o toolchain `cmake/toolchains/llvm-mingw-x86_64.cmake`.

## Primeiro programa

Crie `ola.clp`:

```clp
<< meu primeiro programa
post("Olá, CL++!");
```

```saida
Olá, CL++!
```

Rode com:

```text
clpp ola.clp
```

No VS Code, com o arquivo aberto, use **Ctrl+F5** ou o botão ▶ no canto do editor.

## Como um programa é organizado

Um arquivo `.clp` pode ter, em qualquer ordem:

- `link` — importa outro módulo;
- declarações — `func`, `struct`, `enum`, `variant`, `type`;
- comandos de topo — executados de cima para baixo quando o arquivo é o programa principal.

```clp
link @clpp.axiom as Axiom;

struct Player {
  string name;
  int hp;
}

func heal(int hp) -> int {
  return Axiom.Clamp(hp + 25, 0, 100);
}

Player p = Player("Ada", 90);
post(p.name .: " tem " .: heal(p.hp) .: " de vida");
```

```saida
Ada tem 100 de vida
```

Não existe `main()`: os comandos de topo do arquivo que você executa são o ponto de entrada. Um arquivo importado com `link` (um módulo) não pode ter comandos de topo, só declarações e constantes — assim importar nunca executa código por acidente.

## Linha de comando

| Comando | O que faz |
| --- | --- |
| `clpp arquivo.clp [args...]` | compila e executa; `args()` devolve os argumentos extras |
| `clpp --repl` | modo interativo, um comando por vez |
| `clpp --lsp` | servidor de linguagem (usado pela extensão do VS Code) |
| `clpp --version` | versão |

Erros de compilação e de execução saem no formato `arquivo:linha:coluna: mensagem`, que o VS Code e a maioria dos editores reconhecem como link.
