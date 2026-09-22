# CLUAUPP — product architecture

Cluaupp is the **Roblox project host** for CL++. It is not “another TypeScript → Luau toolchain with more syntax.”

**roblox-ts** already wins on: auto setup, generated Roblox types, Rojo, IntelliSense, watch, transformers, ecosystem size.

**Cluaupp wins by making the toolchain understand Roblox as a platform**, not only as a code target:

```text
roblox-ts  →  TypeScript  →  Luau
Cluaupp    →  CL++ + project semantics  →  validated Roblox project  →  Luau
```

## Split (hard rule)

| Layer | Owns |
| --- | --- |
| **CL++** | Language + compiler: Pest, binder, `TypeId`, checker, Luau emit, source maps, thin platform hooks (`Session::with_roblox_platform`, attrs, future `remote` / context keywords if language-owned) |
| **Cluaupp** | Project model, Rojo/DataModel graph, context/capability policies, remote wiring, API dump refresh, doctor, watch/build graph, Studio layout |
| **Ecosystem** | Rojo, Studio, luau-lsp on **emitted** Luau, Roblox API dump source |

Cluaupp **never** extends `src/ast`. It calls `clpp api` / `clpp lsp` and feeds platform metadata into the session.

## Five pillars

```text
                 CLUAUPP
                    │
     ┌──────────────┼──────────────┐
     │              │              │
     ▼              ▼              ▼
 DataModel       Context        Networking
 Intelligence    Safety         Contracts
     │              │              │
     └──────────────┼──────────────┘
                    │
             Compile-time
              Architecture
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
     Source Debugging     API Intelligence
```

### 1. DataModel intelligence

```text
default.project.json → Rojo / DataModel graph → Cluaupp Project Model → Compiler queries
```

Typed paths such as `UI.Main.PlayButton` resolve to real Instance classes from the project tree. Invalid members / missing children become **compile diagnostics** when the tree is deterministic.

### 2. Context safety (primary differentiator)

Compiler + host understand `@server` / `@client` / `@shared` / `@plugin` as a **capability system**, not lint theater.

- `@client` → `Players.LocalPlayer` ✅  
- `@server` → `Players.LocalPlayer` ❌  
- Client importing a `@server` service without a safe boundary → rejected  

CL++ may own attribute/context **syntax** and checker hooks; Cluaupp owns project-wide policy and script-kind mapping.

### 3. Networking contracts

Single contract → client stub, server stub, validator, names, location, context restrictions:

```text
CL++ contract AST → Client stub | Server stub | Runtime validator
```

Example surface (language RFC later): `remote TradeRequest(itemId: string, amount: number) -> TradeResult`.

### 4. Compile-time architecture / Instance contracts

`@serverOnly` / `@clientSafe` / `@replicated` / `@authority`, call-graph reachability into server-only APIs, and **component schemas** (expected child types under an Instance). Diagnostics are project-level (`CLUAU####`) when they need the DataModel; language codes stay `CLPP####`.

### 5. API intelligence + source debugging + build graph

- `cluaupp api update` / `api diff` — dump → normalize → type registry → compiler + LSP  
- Source maps as a **first-class** requirement (Roblox stack → `.clpp` line)  
- Incremental build graph over client/server/shared/remotes/packages  
- `cluaupp doctor` — whole-project health (API freshness, boundaries, remotes, Instance refs)

## Killer combo (ship order for Cluaupp)

1. **Typed DataModel** (Rojo graph → path typing)  
2. **Server/Client capability system**  
3. **Typed remote contracts**  
4. **Source-level debugging** (maps + doctor)

Do **not** compete with roblox-ts on “more TS features.” Compete on **semantic Roblox validation before play**.

## What CL++ must expose (hooks only)

Already / MVP:

- `Session::language()` vs `Session::with_roblox_platform()`
- Platform prelude / generated `.clh` consumption
- `GetService<T>`, optional narrowing, Instance IS-A
- Emit + source map artifacts for the host

Later language RFCs (only if Cluaupp needs syntax):

- Context attributes as real semantics (`@server` / `@client` …)
- `remote` declarations
- `component` Instance contracts

Prefer expressing these as **Intent / Semantic Interfaces / metaclasses** (RFC 0008) so they are not forever special-cased in `src/ast`.

HIR / query engine / LLVM stay out — see [DEFERRED.md](DEFERRED.md).

## Philosophy one-liner

> Cluaupp turns CL++ into a **semantically validated Roblox project**, then emits idiomatic Luau — zero unnecessary runtime layer.
