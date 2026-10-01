# 10. Módulos: `link` sem cabeçalhos

Em CL++ cada arquivo `.clp` é um módulo. Não existem arquivos de cabeçalho, declarações antecipadas nem ordem de inclusão: o compilador lê o módulo, descobre o que ele declara e expõe isso a quem o importa. O editor faz a mesma coisa enquanto você digita, então tudo que vem de um módulo aparece no autocompletar.

## Importar

```clp trecho
link "./combat.clp" as Combat;   << arquivo do projeto, caminho relativo a este arquivo
link @clpp.axiom as Axiom;       << biblioteca padrão
link @clpp.text;                 << sem `as`: o nome é o último pedaço, com inicial maiúscula (Text)
link "./ui/hud.clp";             << sem `as`: Hud
```

Tudo que o módulo declara é acessado pelo nome dado: `Combat.hit(...)`, `Axiom.Clamp(...)`.

## O que um módulo pode conter

Um módulo só declara coisas; ele nunca executa código ao ser importado. Isso torna a importação previsível e barata.

| Pode | Não pode |
| --- | --- |
| `func`, `struct`, `enum`, `variant`, `type` | comandos de topo (`post(...)`, laços, `let` com cálculo) |
| constantes com valor literal: `const MAX_HP = 100;` | |
| `link` para outros módulos | |

Quando um módulo tem um erro, a mensagem aparece na linha do `link` de quem o importa, com a posição dentro do módulo, e também no próprio arquivo do módulo quando ele está aberto no editor.

## Exemplo completo

O módulo `combat.clp`:

```clp arquivo=combat.clp
<< combat.clp: regras de combate

const MAX_HP = 100;
const CRIT_MULTIPLIER = 2;

struct Fighter {
  string name;
  int hp;
  func isAlive() -> bool { return self.hp > 0; }
}

enum Element { Fire, Ice, Storm }

func damageFor(Element e, int base) -> int {
  match (e) {
    Fire ~> return base + 5;
    Ice ~> return base;
    Storm ~> return base * CRIT_MULTIPLIER;
  }
}

func hit(Fighter target, int amount) -> Fighter {
  target.hp -= amount;
  if (target.hp < 0) { target.hp = 0; }
  return target;
}
```

O programa que usa o módulo:

```clp
link "./combat.clp" as Combat;

Combat.Fighter ada = Combat.Fighter("Ada", Combat.MAX_HP);
let dmg = Combat.damageFor(Combat.Element.Storm, 20);
ada = Combat.hit(ada, dmg);
post(ada.name .: " ficou com " .: ada.hp);
post(ada.isAlive());
```

```saida
Ada ficou com 60
true
```

Detalhes:

- **Funções** são acessadas pelo nome do módulo: `Combat.hit`.
- **Tipos** (structs, enums) podem ser escritos com ou sem o nome do módulo: `Combat.Fighter` e `Fighter` são o mesmo tipo. Escrever com o nome do módulo deixa claro de onde ele vem.
- **Métodos** são chamados no objeto, como sempre: `ada.isAlive()`.
- **Constantes** são substituídas pelo valor na compilação: `Combat.MAX_HP` custa o mesmo que escrever `100`.

## Módulos que importam módulos

Um módulo pode usar `link`. Importações circulares (A importa B que importa A) são detectadas e informadas com o caminho do ciclo.

## Erros comuns

```clp erro
link "./nao_existe.clp" as Missing;
```

```saida
cannot open module
```

Um módulo com código solto no topo é recusado, com a explicação na linha do `link`:

```clp arquivo=solto.clp
post("isto rodaria ao importar");
func ok() { return 1; }
```

```clp erro
link "./solto.clp" as Loose;
```

```saida
a module can only declare
```

## `import` e `using`

`import` é um sinônimo de `link`. `using Modulo.nome;` traz um nome do módulo para o escopo do arquivo, para usá-lo sem prefixo.

```clp
import @clpp.math as Math;
using Math.abs;
post(abs(-7));
```

```saida
7
```

## A biblioteca padrão

| Módulo | Nome sugerido | Conteúdo |
| --- | --- | --- |
| `@clpp.axiom` | `Axiom` | matemática para jogos: interpolação, vetores, ângulos, easing, ruído, cores |
| `@clpp.text` | `Text` | funções de texto |
| `@clpp.math` | `Math` | `abs` |
| `@clpp.fs` | `Fs` | arquivos: `size`, `read`, `list` |
| `@clpp.os` | `Os` | processo: `env` |
| `@clpp.http` | `Http` | URLs: `host` |

Ao digitar `link @clpp.` o editor lista esses módulos com a descrição de cada um. O capítulo 14 detalha as funções.

## Por que não há cabeçalhos

Em C e C++, cabeçalhos existem porque o compilador lê um arquivo de cada vez e precisa saber de antemão o formato de tudo que vem de fora. O preço é alto: declarações duplicadas que saem de sincronia, ordem de inclusão, guardas de inclusão, recompilações em cascata. CL++ resolve isso como C#, Go, Rust e TypeScript: o compilador lê o próprio módulo importado e extrai dele as declarações. Mudou uma assinatura? Quem a usa vê a mudança na hora, no editor e na compilação.
