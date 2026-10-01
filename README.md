<p align="center">
  <img src="assets/brand/clpp-256.png" width="128" alt="Logo do CL++">
</p>

<h1 align="center">CL++</h1>

<p align="center">Linguagem compilada, de uso geral, pensada para jogos.</p>

```clp
link @clpp.axiom as Axiom;

struct Player {
  string name;
  int hp;
  Vector3 position;

  func damage(int amount) {
    @hp = Axiom.Clamp(@hp - amount, 0, 100);
  }
}

Player hero = Player("Ada", 100, Vector3(0, 0, 0));
hero.damage(30);
hero.position.y += 2.5;
post(`${hero.name}: ${hero.hp} hp em ${hero.position}`);
```

```text
Ada: 70 hp em (0, 2.5, 0)
```

- **Compilada para bytecode** verificado e executada numa VM de registradores.
- **Tipagem estática com inferência**: erros de tipo aparecem antes de rodar.
- **Sem cabeçalhos**: cada `.clp` é um módulo; `link` traz funções, tipos e constantes.
- **Feita para jogos**: `Vector2/3/4` nativos, biblioteca Axiom (interpolação, easing, vetores, ângulos, ruído, cores), corrotinas, `signal`, `observable`, tarefas e threads.
- **Janela, gráficos, UI e som embutidos** — sem dependências externas: janela nativa (`@clpp.window`), desenho 2D com texto suave, imagens e efeitos (`@clpp.gfx`), widgets em modo imediato (`@clpp.ui`) e uma UI declarativa estilo Roblox (`@clpp.gui`), som (`@clpp.audio`), console (`@clpp.io`), automação de teclado/mouse para macros (`@clpp.input`), JSON (`@clpp.json`) e datas (`@clpp.time`). Veja os capítulos [17](docs/guia/17-janela-e-graficos.md), [18](docs/guia/18-interface.md) e [19](docs/guia/19-audio-io-automacao.md).
- **Editor completo**: extensão do VS Code com erros ao digitar, autocompletar (inclusive membros herdados e de módulos), hover, ir para definição, renomear, amostras de cor para `0xRRGGBB` e executar com Ctrl+F5.

## Instalar

**Windows:** baixe `clpp.exe` e `clpp-language-<versão>.vsix` em Releases. No VS Code: **Extensões → ⋯ → Instalar do VSIX…**. A extensão já traz o compilador.

**A partir do código** (CMake 3.20+, Ninja, compilador C++20):

```text
cmake --preset release
cmake --build --preset release
./build/release/src/clpp examples/hello.clp
```

## Documentação

O [guia da linguagem](docs/README.md) tem 16 capítulos, do primeiro programa à concorrência, com exemplos que a suíte de testes executa. As mudanças desta revisão estão em [docs/CHANGES.md](docs/CHANGES.md).

## Desempenho

Mediana de 7 execuções, Xeon 2,1 GHz, em segundos ([detalhes](docs/guia/16-desempenho.md)):

| Programa | Antes | Agora | CPython 3.11 |
| --- | --- | --- | --- |
| `fib(27)` | 1,605 | 0,095 | 0,031 |
| laço de 3 milhões | 0,379 | 0,180 | 0,264 |
| 300 mil chamadas virtuais | 0,595 | 0,109 | 0,035 |
| 200 mil passos de física | 0,353 | 0,050 | 0,087 |
| 100 mil templates | 0,034 | 0,023 | 0,024 |

## Estrutura

| Pasta | Conteúdo |
| --- | --- |
| `src/core/` | lexer, parser, binder, tipos, codegen, VM, IDE/LSP |
| `src/stdlib/` | biblioteca padrão nativa |
| `src/cli/` | executável `clpp` |
| `include/clpp/` | API C++ para embutir |
| `tools/vscode/` | extensão do VS Code |
| `examples/` | programas de exemplo |
| `benchmarks/` | programas de medição (CL++ e Python) |
| `tests/` | unitários, regressões, exemplos, stress, LSP, extensão, documentação |
| `docs/` | guia e referência |

## Licença

Ver [LICENSE](LICENSE).
