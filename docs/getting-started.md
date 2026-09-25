---
title: Getting started
description: Install CL++, link modules, and compile a script.
---

# Getting started

CL++ compiles to Luau. Modules use `link`. `#include` and `import` are rejected (`use link`).

## Install

Install [CL++ 0.8+](https://github.com/KartzRbx/CLPP/releases) so `clpp` is on your PATH. The new pipeline binary, until it replaces that command, is `clpp_cli`.

File tags choose the run context passed to the checker:

| File | `RunContext` |
| --- | --- |
| `*.client.clpp` | Client |
| `*.server.clpp` | Server |
| anything else | Module |

## A script

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link @game.ReplicatedStorage.Modules.Combat as CombatModule;

@server
void Save();

void init() {
    Janitor janitor;
    janitor.Add(CombatModule);
    Save();
}
```

`link @clpp…` is the standard-library prelude. `link @game…` becomes `game:GetService` plus `require`. A relative module is `link "./Components/Health" as Health;`.

## Safety

The checker reads `@client`, `@server`, and `@parallel`.

- A Client file that calls an `@server` function is `CLUAU_AUTH001`.
- An assignment inside `@parallel` is `CLUAU_PAR001`.

`clpp_cli build` does not emit Luau when either diagnostic is present. `clpp_cli lsp <file>` prints those diagnostics for the editor.

```bash
clpp_cli build src/Boot.server.clpp
clpp_cli lsp src/Boot.client.clpp
```
