# Toolchain e ferramentas

Infraestrutura alinhada a linguagens de produção modernas: build rápido, testes por estágio da pipeline e diagnóstico de memória/qualidade no CI e no dev local.

| Categoria | Ferramenta | Motivo |
|-----------|------------|--------|
| Linguagem base | **C++20** (GCC 13+, Clang 16+, MSVC 19.29+) | `std::span`, concepts, performance nativa, base para VM e GC |
| Build | **CMake 3.20+** + **Ninja** | Presets em `CMakePresets.json`; Ninja acelera rebuild incremental |
| Testes | **Catch2 v3** (FetchContent) | Testes rápidos por estágio: lexer, parser, VM |
| Qualidade / memória | **AddressSanitizer** + **clang-tidy** | Opções `CLPP_SANITIZE_ADDRESS`, `CLPP_ENABLE_CLANG_TIDY` |
| IDE | **clangd** + extensão VS Code | `compile_commands.json` + `.clangd`; grammar em `tools/vscode/` |

## Requisitos mínimos

- CMake ≥ 3.20
- Ninja (recomendado)
- Compilador C++20 (Clang 16+ ou GCC 13+ no Linux/macOS; MSVC no Windows)
- Opcional: `clang-tidy`, `clangd`

### Windows (instalação rápida)

No PowerShell (admin opcional):

```powershell
winget install Kitware.CMake Ninja-build.Ninja LLVM.LLVM
```

**Feche e abra de novo o terminal** (ou o Cursor) para o `PATH` atualizar.

Nesta máquina o compilador usado é o **GCC do MSYS2 UCRT64** (`C:\msys64\ucrt64\bin\g++.exe`). O preset `ucrt64` já aponta para ele. Se o MSYS2 estiver em outro caminho, edite `cmake/toolchains/ucrt64-gcc.cmake`.

## Comandos habituais

No **PowerShell**, use o preset (evite `-D...` solto — o PS interpreta como parâmetro próprio):

```powershell
cmake --preset ucrt64
cmake --build --preset ucrt64
ctest --preset ucrt64 --output-on-failure
```

Com **Visual Studio** / **Clang** no PATH, o preset `debug` basta:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Build com ASan
cmake --preset asan
cmake --build --preset asan
ctest --preset asan

# Análise estática no build (Clang/GCC)
cmake --preset tidy
cmake --build --preset tidy
```

## Variáveis CMake

| Variável | Default | Descrição |
|----------|---------|-----------|
| `CLPP_BUILD_TESTS` | `ON` | Suíte Catch2 |
| `CLPP_SANITIZE_ADDRESS` | `OFF` | AddressSanitizer em todos os alvos. O preset `asan` liga `-lasan`. O GCC do MSYS2 UCRT64 no Windows muitas vezes não tem `libasan`, então esse preset falha na ligação. Rode o ASan com Clang ou no CI do Linux; a falha no UCRT64 não é um defeito do produto. |
| `CLPP_ENABLE_CLANG_TIDY` | `OFF` | Roda clang-tidy ao compilar |

## VS Code

- Extensões sugeridas: `llvm-vs-code-extensions.vscode-clangd`, `ms-vscode.cmake-tools`
- Desative o IntelliSense da extensão Microsoft C/C++ se usar **clangd** como LSP principal (evita conflito).
- `CompilationDatabase` aponta para `build/debug` após `cmake --preset debug`.
- Arquivos `.clp` usam a extensão em `tools/vscode` (`clpp --lsp`): diagnóstico, conclusão, hover e definição. Veja `tools/vscode/README.md`.
