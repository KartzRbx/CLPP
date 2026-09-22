# Relatório COMPLETO de mudanças — CL++ 0.8

Tudo o que mudou nesta linha de trabalho, sem omitir camadas.

| Campo | Valor |
| --- | --- |
| Versão alvo | **0.8.0** |
| Release anterior | 0.7.0 (`773f9f8`) |
| Commit base já no git (desta linha) | `3461bb2` — *Ship RFC 0011 opt pipeline Phase 0–1 lite, Option/Result, and fair bench fixtures* |
| Working tree atual | **ahead 1 / behind 5** vs `origin/main` + **~37 arquivos modificados** + **~17 untracked** além do que `3461bb2` já trouxe |
| Escopo permanente fora | LLVM / 2º JIT, templates C++ como modelo, GC/VM própria, lexer novo, Phase C research |

Este documento cobre **duas camadas**:

1. **Camada A — `3461bb2`** (já commitada): ~132 arquivos, +12817 / −1807 linhas — a fundação 0.8 parcial.
2. **Camada B — working tree pós-`3461bb2`** (ainda não commitada nesta máquina): +1110 / −100 nos tracked + arquivos novos listados abaixo — backlog do plano + grammar P0.

---

# PARTE A — O que `3461bb2` já shippou (fundação)

## A.1 Arquitetura e pacotes

- Pipeline documentado: Pest → AST → binder → TypeId/checker → opts AST (RFC 0011) → emit Luau.
- Separação linguagem vs Roblox: `Session::language()` vs `Session::with_roblox_platform()`; `src/platform`, `src/roblox`.
- `src/semantic` → `src/names.rs`; módulos novos: `types/`, `symbols/`, `session/`, `modules/`, `opt/*`, RFCs 0001–0012 (parcial), docs `architecture/*`.
- Artifact JSON: `nativeHints`, `layoutHints`, `specialized`, `optimized`, `sourceMap`.
- Flag `--no-opt` / `CompileRequest.optimize: false` para baseline fair.

## A.2 Opt pipeline Phase 0–1 lite (já em `3461bb2`)

| Módulo | Função |
| --- | --- |
| `opt/fold.rs` | constant fold, prop, DCE de `if (false)` |
| `opt/inline.rs` | inline de funções pequenas / `@noinline` |
| `opt/scalar.rs` | scalar replacement InitList + Vector escape-aware |
| `opt/escape.rs` | helpers Vector2/3 |
| `opt/loop_opt.rs` | hoist `const` invariante de loop |
| `opt/mono.rs` | monomorphization com type args **explícitos** |
| `opt/layout.rs` | candidatos Dense/SoA/buffer → `layoutHints` |
| `opt/native.rs` | candidatos `@native` seletivos → `nativeHints` |
| `opt/mod.rs` | orquestra a pipeline e `OptReport` |

## A.3 Option/Result subset (já em `3461bb2`)

- Gramática / emit smoke: `Ok`/`Err`/`Some`/`None`, tipo `Result<T,E>`.
- Ainda **não** fechava nesta camada: `?` completo, match exaustivo, CLPP0202/1101/1102 (isso é Parte B).

## A.4 Modules / session / docs / benches (já em `3461bb2`)

### A.4.1 Nova forma canônica: `import { … } from` (substitui `#include` como superfície de linguagem)

Antes, dependências entre arquivos CL++ eram sobretudo **`#include`** (preprocess text splice / require legado). Isso misturava:

- splice textual (mesmo stem header/impl),
- `require()` para stems diferentes,
- angle includes de plataforma (`<clpp/…>`).

A forma **canônica da linguagem** passou a ser o import nomeado estilo TypeScript (RFC 0003):

```clpp
import { Wallet } from "./PlayerData.clh";
import { PlayerData as Data } from "./PlayerData.clh";
import { Wallet, PlayerData } from "./PlayerData.clh";
```

| Antes (`#include`) | Depois (`import`) |
| --- | --- |
| Preprocess / splice ou require opaco | Item de AST `Item::Import` |
| Traz “o arquivo inteiro” (ou require sem lista) | Traz **só os nomes listados** (+ membros) |
| Sem rename de símbolo | `as` renomeia no consumidor |
| Sem grafo tipado de exports | `merge_exports` + `TypeDatabase` compartilhada |
| Ciclo = recurse/`seen` silencioso | Ciclo → **CLPP1001** (Parte B) |
| Path missing = erro preprocess | Path missing → **CLPP0801** |

**Grammar** (`import_decl`):

```pest
import_decl = {
    KW_IMPORT ~ "{" ~
        import_binding ~ ("," ~ import_binding)* ~ ","? ~
    "}" ~ KW_FROM ~ string ~ ";"
}
import_binding = { ident ~ (KW_AS ~ ident)? }
```

**Não existe `export`**: todo top-level do arquivo módulo (struct/class/interface/enum/type/using/funções livres) é importável. Privacidade é `private:` dentro de struct, não keyword de arquivo.

**Emit:** cada import vira entrada em `CompileContext.requires` → `require(...)` no Luau (Cluaupp/Rojo mapeiam path).

**`#include` continua só como legado / host:**

```clpp
#include "PlayerData.clh"      // stem diferente → ainda require()
#include "Main.clh"            // mesmo stem que Main.clpp → splice header/impl
#include <clpp/roblox.clh>     // prelude Cluaupp — NÃO é módulo de linguagem
```

Regra prática: **código novo = `import`**. Angle `#include <…>` só para headers de engine/gerados do Cluaupp.

Docs canônicos: [`docs/modules.md`](modules.md), [`docs/reference/import.md`](reference/import.md), RFC 0003.

### A.4.2 Resto (session / benches / docs)

- Session com cache por hash de arquivo.
- Fixtures: `benches/numeric.clpp`, `data.clpp`, `realistic.clpp`, `tests/fair_bench.rs`, `tests/opt_fold.rs`.
- Docs massivos: `docs/architecture/*`, `docs/modules.md`, `docs/benchmarks*`, RFCs 0001–0011 (+ 0012 subset), site/www updates.
- Exemplos legados removidos/ajustados; golden Leaderstats/hello limpos.

## A.5 Stats brutos `773f9f8..3461bb2`

~132 files, **+12817 / −1807** lines (inclui docs, Cargo.lock, roblox metadata, tests).

---

# PARTE B — Working tree pós-`3461bb2` (tudo que ainda não está no commit remoto desta máquina)

## B.0 Contagem

| Tipo | Quantidade |
| --- | --- |
| Tracked modificados | 37 |
| Untracked novos | 17 (+ o próprio este MD) |
| Diff tracked vs `HEAD` (`3461bb2`) | **+1110 / −100** |

---

## B.1 Build / versão — `Cargo.toml` / `Cargo.lock`

| Mudança | Detalhe |
| --- | --- |
| `version` | `0.7.0` → **`0.8.0`** |
| Feature `cli` | `[[bin]] required-features = ["cli"]` |
| Features | `default = ["cli"]`, `cli = ["dep:clap"]` |
| `clap` | passou a `optional = true` |
| Motivo | permitir `cargo test --no-default-features` sem linkar o bin (disco apertado / CI parcial) |
| `Cargo.lock` | regenerado conforme features |

---

## B.2 Grammar — `src/parser/grammar.pest` (P0 auditoria + try)

### Removido
- `KW_EXPORT` e entrada em `kw` (RFC 0003: export implícito).
- Produção `get_service` (Roblox fora do PEG).
- `try_suf` dentro de `postfix` (ambiguidade com ternário).

### Adicionado / reestruturado

**Precedência de expressões (ligação fraca → forte):**

```text
assign
  → try_expr          # Result: expr?
    → colon_expr      # Roblox: obj:method
      → ternary       # cond ? then : else
        → coalesce    # ??
          → shift / or / and / cmp / concat / add / mul / unary / pow
            → primary → postfix   # sem try, sem colon
```

Regras novas:
- `try_expr = { colon_expr ~ try_op? }`
- `try_op = { "?" ~ !"." ~ !"?" }`
- `colon_expr = { ternary ~ colon_suf* }`

**`function_type`:** de `type_spec? ~ ("," ~ type_spec)*` para `(type_spec ~ ("," ~ type_spec)*)?` (sem vírgula inicial).

**`atom`:** `GetService<T>()` cai em `generic_call`.

### Bugs estruturais corrigidos
1. `cond ? a : b` era parseado como then-branch `a:b` (`colon_suf`).
2. Lookaheads frágeis de `try_suf` vs ternário com whitespace — substituídos por camadas.
3. Keyword `export` sem produção.

---

## B.3 Parser — `src/parser/mod.rs`

| Mudança | Detalhe |
| --- | --- |
| `parse_expr` | arms `Rule::try_expr`, `Rule::colon_expr` |
| `parse_try_expr` | aplica `Expr::Try` após ternário/colon |
| `parse_colon_expr` | aplica `colon_suf` via `apply_postfix` |
| Removido | arm `Rule::get_service` (virava Call manual) |
| Removido | arm `Rule::try_suf` no postfix |
| `GetService` | agora só via `generic_call` → `Expr::Call { type_args }` |

---

## B.4 AST — `src/ast/mod.rs` + `visit.rs`

```rust
// Novo em Expr:
Try { argument: Box<Expr> }  // RFC 0012: expr?
```

- `visit.rs`: visita `Expr::Try` (walk do argument).
- Comentário de doc no enum.

---

## B.5 Binder — `src/binder/mod.rs`

- `Expr::Try` incluído no grupo unary-like de `bind_expr`.
- Match arms: bindings de tags `Ok`/`Err`/`Some`/`None` usam `declared_type: None` (payload), **não** o nome da tag como tipo (evita “cannot assign Ok to int”).

---

## B.6 Types — `src/types/mod.rs` + `parse.rs`

| API nova | Papel |
| --- | --- |
| `TypeDatabase::result(ok, err)` | nominal `Result<labelOk,labelErr>` |
| `is_result` | peel + prefix `Result<` |
| `unwrap_result_labels` | split top-level comma respeitando `<>` |
| `split_top_comma` | helper privado |

`parse.rs`: genérico `Result` chama `db.result` em vez de `intern` manual.

---

## B.7 Checker — unificação + Result + match + `?`

### `src/checker/mod.rs`

- **`check_unified(program, source, libraries, bound, types)`**
  - Roda `check_typed` (autoritativo).
  - Mergeia `check_program_ex` (pass) só onde não há código/mensagem typed na mesma linha (evita ruído optional/Result).
- `type_of_expr`:
  - `None` → nil
  - `Some(x)` → optional(inner)
  - `Ok(v)` / `Err(e)` → `result(...)`
  - `Try` → unwrap label Ok (ou inner)

### `src/checker/typed.rs`

- `Stmt::Match`: check discriminant + **`check_match_exhaustiveness`**
  - Result → precisa `Ok`+`Err` ou `_` → senão **CLPP1102**
  - Option/optional → `Some`+`None` ou `_`
- `Expr::Try`: se arg não é Result-ish → **CLPP1101**
- Assign Result → non-Result → **CLPP0202** (já preparado em assign site)
- Walk continua nos bodies dos arms

### `src/checker/pass.rs`

- Match `Expr::Try` em `expr_ty` (não-exaustivo coberto).

### `src/compile.rs`

- **Remove** chamada solta a `check_program_ex` antes do bind.
- Usa só `check_unified` após `resolve`.

---

## B.8 Diagnósticos — `src/diag.rs`

| Código | Título | Help (resumo) |
| --- | --- | --- |
| **CLPP0202** | Result assigned to non-Result | match / `?` / mudar tipo |
| **CLPP1001** | import cycle detected | quebrar ciclo |
| **CLPP1101** | try (?) on non-Result | precisa Result |
| **CLPP1102** | non-exhaustive Option/Result match | Ok+Err ou Some+None ou `_` |

`all()` / `explain()` atualizados.

---

## B.9 Codegen — `src/codegen/emit/engine.rs`

| Área | Mudança |
| --- | --- |
| `Expr::Try` | IIFE: se `_r.err` early-return `{ err }`, senão `_r.ok` |
| `Ok` / `Err` / `Some` / `None` | `{ ok= }`, `{ err= }`, valor, `nil` (já parcial; reforçado) |
| `emit_match` | tags: `Ok`→`.ok ~= nil`, `Err`→`.err`, `Some`→`~= nil`, `None`→`== nil`; binding unwrap `.ok`/`.err` |
| Walk | `Try` em cleanup/walk |
| `inferred_type` | `Option<>`/`optional<>` → `T?`; `Result<>` → `any` (tagged) para native |
| Buffer emit | `ArrayLit` numérico ≥8 → `buffer.create` + `buffer.writef64` + comentário `BufferSpecialize` |

---

## B.10 Otimizações — Parte B (além do que `3461bb2` já tinha)

### Novos arquivos

#### `src/opt/soa.rs`
- Detecta local `array` de ≥2 `InitList` com mesmos campos.
- Reescreve em arrays paralelas `name__field` (`array<float>`).
- Emite notas `SoA:name` → `layoutHints`.

#### `src/opt/buffer_spec.rs`
- Varre decls com `ArrayLit` de números ≥8.
- Produz strings `BufferSpecialize:name` → `layoutHints`.
- Emit consome isso (ver B.9).

### Modificados

| Arquivo | Mudança |
| --- | --- |
| `opt/mod.rs` | `mod soa` / `buffer_spec`; pipeline: … → `soa::run` → … → `layout` + soa notes + buffer notes |
| `opt/mono.rs` | **`infer_type_args`**: mono sem `Foo<T>` explícito no call site; `Try` no walk/rewrite |
| `opt/inline.rs` | heurística **custo/benefício**; visita `Try` |
| `opt/fold.rs` | strength reduction `ident*2` → `ident+ident` |
| `opt/escape.rs` | escape analysis geral (`escapes` / stmt/expr); campos Color3/CFrame; visita `Try` |
| `opt/loop_opt.rs` | `Try` em `expr_invariant`; fix pattern Coalesce sem `op` |
| `opt/scalar.rs` | `Try` em `expr_uses_whole` |

Ordem atual da pipeline:

```text
fold/DCE → inline → fold/DCE → scalar → soa → loop → mono → fold/DCE
→ native hints → layout + soa notes + buffer_spec
```

---

## B.11 Modules — `src/modules/mod.rs`

Endurecimento sobre o import da Parte A:

| API / fluxo | Papel |
| --- | --- |
| `import_named` | resolve path relativo, carrega módulo filho, `merge_exports` só dos nomes pedidos |
| `attach_named_requires` | preenche `CompileContext.requires` para emit `require` |
| `merge_exports` | copia símbolos top-level (+ membros) para o consumidor; honra `as` |
| **`detect_cycles`** | DFS com stack; arestas de ciclo → **CLPP1001** |
| `resolve_quoted_include` | mesmo resolvedor de path que `#include "..."` legado |

Fluxo mental:

```text
parse import_decl
  → Item::Import { names, module }
  → Session/modules::import_named
       → resolve "./Foo.clh"
       → load + resolve types no TypeDatabase
       → merge_exports (só nomes listados)
       → detect_cycles → CLPP1001 se ciclo
  → attach_named_requires → artifact.requires → Luau require()
```

`#include` **não** foi apagado do preprocess: ainda existe para splice mesmo-stem e angle platform. A **superfície de linguagem** para módulos entre arquivos CL++ é só `import`.

---

## B.12 Session — `src/session/mod.rs`

| Campo/API | Papel |
| --- | --- |
| `deps: HashMap<String, HashSet<String>>` | arestas import/require |
| `epoch: u64` | bump em invalidate |
| `invalidate(path)` | remove arquivo + **dependentes** transitivos |
| `dep_graph()` | leitura do grafo |

Ainda é RFC 0006 **lite** (não Salsa / query cache disco).

---

## B.13 Platform — `src/platform/mod.rs`

```rust
enum HostCapability { NativeCandidate, BufferCandidate, SoaCandidate, ServerOnly, ClientOnly }
fn capabilities_from_hints(native, layout) -> Vec<(String, HostCapability)>
```

Mapeia prefixes: `BufferSpecialize:`, `BufferCandidate:`, `SoA:`, `SoACandidate:`, `DenseNumeric:`, etc.

Docs no módulo sobre artifact hooks para Cluaupp.

---

## B.14 Intent — `src/intent/mod.rs`

| Item | Conteúdo |
| --- | --- |
| `is_option_result_builtin` | Some/None/Ok/Err |
| `OPTION_RESULT_BUILTINS` | const slice |
| `INTENT_RESERVED` | requires, ensures, effect, pure, newtype, unique, move |
| `EffectTag` | Pure/Io/Yields/Mutates + `parse_effect_tag` |
| `TypeState` / `Newtype` | structs placeholder RFC 0016/0017 |
| Docs de módulo | aponta RFCs 0009–0019 |

---

## B.15 Analysis / LSP

### `src/analysis/mod.rs`
- `RenameResponse`, `TextEdit`, **`rename_request`**
- Completion: Option/Result + Intent reserved
- Hover: textos Markdown para Some/None/Ok/Err
- (references/definition já existiam; rename reusa references)

### `src/lsp/server.rs`
Capacidades novas no `initialize`:
- `references_provider`
- `rename_provider`
- `signature_help_provider` (`(` / `,`)
- `document_formatting_provider`

Handlers: `references`, `rename` → `WorkspaceEdit`, `signature_help`, `formatting` → `fmt::format_source`.
Helper `loc_to_lsp`.

---

## B.16 ICE — `src/ice.rs` (novo) + `lib.rs`

```rust
pub struct IceReport { message, location, hint }
pub struct ArenaHint; // placeholder deferred
```

`pub mod ice` em `src/lib.rs`.

---

## B.17 RFC / docs novos e atualizados

### Novos RFCs
| Arquivo | Tema | Status |
| --- | --- | --- |
| `rfc/0013-pattern-matching.md` | match rico / tags Option-Result | Accepted subset |
| `rfc/0014-contracts.md` | requires/ensures | Draft |
| `rfc/0015-effects-lite.md` | effects | Draft |
| `rfc/0016-typestate.md` | typestate | Draft |
| `rfc/0017-nominal-types.md` | newtype | Draft |
| `rfc/0018-move-semantics.md` | move | Draft |
| `rfc/0019-intent-phase-b.md` | Phase B | Deferred |

### RFCs atualizados
- `rfc/0003-module-system.md` — sem keyword `export`; export implícito.
- `rfc/0012-option-result.md` — `?` e match exaustivo passaram de “later” para **shipping**; testes apontam `option_result.rs`.

### Docs novos
| Arquivo | Conteúdo |
| --- | --- |
| `docs/option-result.md` | guia Option/Result/`?`/match |
| `docs/cluaupp-host.md` | como cluaupp consome artifact |
| `docs/benchmarks/studio-suites.md` | suites A–F + baselines |
| `docs/CHANGELOG-0.8-DETAILED.md` | **este relatório** |
| `CHANGELOG.md` | release notes curtas + link |

### Docs atualizados
- `docs/architecture/FEATURE_CHECKLIST.md` — matriz feature×camadas 0.8
- `docs/architecture/DEFERRED.md` — ICE/arena stub
- `docs/benchmarks/results.json` + `www/public/benchmarks/results.json` — `studio_suites` com métricas **null** + catch CLPP0202
- `src/README.md` — GetService = generic_call

---

## B.18 Cluaupp contract / bridge

### `support/cluaupp.d.ts`
- Header **0.8.0**
- Campos já presentes: `optimize`, `nativeHints`, `layoutHints`, `specialized`, `optimized`

### `support/cluaupp-bridge.ts` (novo)
| Função | Papel |
| --- | --- |
| `applyNativeHints` | injeta `@native` só em funções hintadas |
| `planLayout` | separa soa vs buffer a partir de `layoutHints` |
| `hotFunctions` | PGO threshold |
| `CluauppProduct` | stubs watch / Rojo / remotes / DataModel / doctor / sourceMap |

---

## B.19 Testes — `tests/option_result.rs` (novo)

| Teste | Assert |
| --- | --- |
| `ok_err_emit_and_try` | compile Ok + `?` |
| `result_assign_to_plain_is_error` | **CLPP0202** |
| `match_result_exhaustive_ok` | match Ok/Err ok + emit `.ok` |
| `match_result_non_exhaustive` | **CLPP1102** |
| `try_on_non_result` | **CLPP1101** |
| `option_some_none_match` | match Some/None |
| `ternary_and_try_do_not_conflict` | ternário com espaços/idents + `r?` + return ternário |

Suites existentes que devem continuar verdes com Parte B: `opt_fold`, `v07` (incl. ternário), `fair_bench`, `complete`, `check`, `api`.

---

## B.20 Inventário arquivo a arquivo (Parte B)

### Modificados (tracked)

```
Cargo.lock
Cargo.toml
docs/architecture/DEFERRED.md
docs/architecture/FEATURE_CHECKLIST.md
docs/benchmarks/results.json
rfc/0003-module-system.md
rfc/0012-option-result.md
src/README.md
src/analysis/mod.rs
src/ast/mod.rs
src/ast/visit.rs
src/binder/mod.rs
src/checker/mod.rs
src/checker/pass.rs
src/checker/typed.rs
src/codegen/emit/engine.rs
src/compile.rs
src/diag.rs
src/intent/mod.rs
src/lib.rs
src/lsp/server.rs
src/modules/mod.rs
src/opt/escape.rs
src/opt/fold.rs
src/opt/inline.rs
src/opt/loop_opt.rs
src/opt/mod.rs
src/opt/mono.rs
src/opt/scalar.rs
src/parser/grammar.pest
src/parser/mod.rs
src/platform/mod.rs
src/session/mod.rs
src/types/mod.rs
src/types/parse.rs
support/cluaupp.d.ts
www/public/benchmarks/results.json
```

### Novos (untracked)

```
CHANGELOG.md
docs/CHANGELOG-0.8-DETAILED.md
docs/benchmarks/studio-suites.md
docs/cluaupp-host.md
docs/option-result.md
rfc/0013-pattern-matching.md
rfc/0014-contracts.md
rfc/0015-effects-lite.md
rfc/0016-typestate.md
rfc/0017-nominal-types.md
rfc/0018-move-semantics.md
rfc/0019-intent-phase-b.md
src/ice.rs
src/opt/buffer_spec.rs
src/opt/soa.rs
support/cluaupp-bridge.ts
tests/option_result.rs
```

---

# PARTE C — Superfícies de linguagem (estado 0.8 combinado A+B)

## C.1 Option / Result

```clpp
optional<int> o = Some(1);
Result<int, string> r = Ok(1);
int x = r?;                    // Try
match (r) {
  Ok v => { return v; },
  Err e => { return 0; },
}
```

Emit try (simplificado):

```lua
(function() local _r = ...; if _r.err ~= nil then return { err = _r.err } end; return _r.ok end)()
```

## C.2 Módulos — migração `#include` → `import`

### Forma nova (usar sempre em código novo)

```clpp
import { Wallet } from "./PlayerData.clh";
import { PlayerData as Data } from "./PlayerData.clh";
```

- Só os nomes entre `{ }` entram no escopo do arquivo.
- `as` cria alias local.
- Sem `export`: top-level do `.clh`/`.clp` já é exportável.
- Path relativo ao arquivo que importa.
- Ausente → **CLPP0801**; ciclo → **CLPP1001**.

### Forma antiga (legado / host)

```clpp
#include "PlayerData.clh"       // ainda aceito; preferir import
#include "Main.clh"             // splice se mesmo stem do .clpp
#include <clpp/roblox.clh>      // só plataforma Cluaupp
```

| Precisa de… | Use |
| --- | --- |
| Tipos/funções de outro arquivo CL++ | `import { … } from "…"` |
| Par header/impl mesmo stem | `#include "Foo.clh"` no `Foo.clpp` (legado) |
| API Instance / prelude Roblox | `#include <clpp/…>` via Cluaupp, **não** como módulo de app |

### O que o Luau vê

```lua
local Wallet = require(...)  -- path Rojo/Cluaupp a partir de requires[]
```

Não há `export` no Luau gerado além do que o módulo já retorna; a seleção de nomes é no **checker**/merge, não um `export` keyword CL++.

## C.3 GetService (semântica host, sintaxe genérica)

```clpp
Players p = GetService<Players>();  // generic_call → Call + type_args
```

## C.4 Precedência crítica

```text
a = b = c
a?
game:GetService(...)
c ? t : e
a ?? b
|| && == ... + * unary **
```

---

# PARTE D — Mapa plano de backlog → status

| ID do plano | Status nesta linha |
| --- | --- |
| a1-unify-checker | Feito (`check_unified`) |
| a1-result-typing | Feito (CLPP0202 + construtores + Try type_of) |
| a1-mono-infer | Feito (`infer_type_args`) |
| a2-result-match | Feito (CLPP1102 + emit tags) |
| a2-try-op | Feito (`Expr::Try` + grammar camadas) |
| a2-result-docs-tests | Feito (`docs/option-result.md`, `tests/option_result.rs`) |
| a3-soa-codegen | Feito (`opt/soa.rs`) |
| a3-buffer-codegen | Feito (hints + emit buffer) |
| a3-loop-licm | Parcial (hoist const + strength `*2`; LICM pleno limitado) |
| a3-inline-cost | Feito |
| a3-escape-general | Feito (API escapes + mais datatypes) |
| a4-preserve-types-emit | Feito (Option/Result anotações) |
| a4-modules-cycles | Feito (CLPP1001) |
| a4-platform-hooks | Feito (`capabilities_from_hints`) |
| a5-lsp-rename-refs | Feito |
| a5-lsp-format | Feito |
| a5-lsp-gaps | Feito (Option/Result/Intent reserved) |
| a5-diag-explain | Feito (0202/1001/1101/1102) |
| a6-incremental-rfc0006 | Lite (deps+invalidate; não Salsa) |
| a6-arena-parallel-lowpri | Stub (`ice.rs`) |
| a7-feature-checklist | Matriz 0.8 |
| a7-cluaupp-contract | d.ts 0.8 + bridge |
| a7-starlight | prepare-starlight rodou quando www existia |
| b-rfc-pattern-match … b-rfc-move | RFCs + reserved; **impl full não** |
| c-phase-b-regions | RFC 0019 deferred |
| d-cluaupp-* | Bridge TS de referência; host real externo |
| e-studio-benches / publish | fixtures + JSON null honesto |
| f-sync-origin | **Pendente** (ahead/behind; stash/rebase incompleto) |
| f-ci-full-suite | Parcial (disco; testes chave verdes com `--no-default-features`) |
| f-release-changelog | `CHANGELOG.md` + este doc; versão 0.8.0 no Cargo |

---

# PARTE E — O que NÃO mudou / dívida explícita

1. Dual `template<typename T>` + `Foo<T>` no grammar — sem decisão final.
2. `skip_decl` (typedef/extern) ainda engole sem semântica.
3. `namespace` ainda parseia e gera CLPP0301.
4. `new` só `ident`, sem `Namespace::Foo<T>`.
5. `function_item` só um `::`.
6. Match ainda sem ADTs aninhados / guards / literais.
7. Numbers/strings sem hex/scientific/`\n` em template escapes (auditoria P1+).
8. Union `|` / intersection `&` sem níveis de precedência.
9. Studio P50/P95/P99 — **null** até medição real.
10. Sync com `origin/main` (behind 5) — não concluído.
11. Intent 0014–0018 — só RFC/reserved, não grammar+checker completos.

---

# PARTE F — Como validar

```bash
cargo test --offline --no-default-features --test option_result
cargo test --offline --no-default-features --test v07
cargo test --offline --no-default-features --test opt_fold
cargo test --offline --no-default-features --test fair_bench
cargo test --offline --no-default-features --test complete
```

Com CLI:

```bash
cargo build --features cli
clpp --version   # 0.8.0
```

---

# PARTE G — Totais aproximados

| Camada | Escala |
| --- | --- |
| A (`3461bb2` vs 0.7) | ~132 files, +12.8k / −1.8k |
| B (WT vs `3461bb2`) | 37 modified + 17 new, +1.1k / −0.1k tracked |
| **Linha 0.8 completa (A+B)** | fundação opt/modules/types **mais** checker unificado, Result fechado, SoA/buffer, LSP, RFCs Intent, grammar P0, docs/contrato |

---

*Este arquivo é a fonte detalhada. O `CHANGELOG.md` na raiz é o resumo executivo e aponta para cá.*
