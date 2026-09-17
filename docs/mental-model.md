---
title: Mental model
---

# Mental model

Hold these five rules and the rest of the language clicks.

## 1. One file in, one file out

`LeaderstatsServer.server.clpp` → `LeaderstatsServer.server.luau` (Script).  
`Hud.client.clpp` → LocalScript.  
`config.clp` with no tag → ModuleScript.

## 2. Four accessors, never `->`

| You write | You mean |
| --- | --- |
| `player.Name` | property |
| `player::FindFirstChild("x")` | method |
| `DataService:Server` | table key |
| `a .: b` | string join |

## 3. Pointers are Instances

`Player*` is a Roblox `Player`. There is no `delete`, no `*player`, no `player->Name`. Lifetime is `Destroy` or Janitor.

## 4. `init` starts a script; modules return tables

Scripts: define `void init()`.  
Modules: usually **do not** define `init()` unless the `require` should run it.

## 5. CL++ is the language; Cluaupp is the engine cable

If a Roblox class is missing from IntelliSense, that is a Cluaupp generated-header problem, not a CL++ syntax problem.

## What the compiler emits

- `local` / `const` / `const function`
- `Instance.new` / `game:GetService` / `require`
- `--!strict` only with `#pragma strict`

Formatting is StyLua's job. Typechecking emitted Luau is `luau-analyze`'s job.

Next: [Files, tags, includes](files).
