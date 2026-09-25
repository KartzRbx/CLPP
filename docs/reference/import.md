---
title: "link"
sidebar_label: "link"
---

# link

<div class="clpp-ref-meta">Modules</div>

The only module form. `import` and `#include` do not parse.

## Syntax

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link @game.ReplicatedStorage.Modules.Combat as CombatModule;
link "./Components/Health" as Health;
```

## Parameters

| Name | Description |
| --- | --- |
| `@clpp…` | Standard library path |
| `@game…` | DataModel path from the Rojo project |
| `"./…"` | Path relative to this file |
| `as Alias` | Name bound in this file |

## Luau emit

`@game` becomes `GetService` plus `require`. `@clpp` is a prelude and does not emit `require`. A relative path becomes `require`.

## Errors

| Code | When |
| --- | --- |
| `` use `link` `` | The file uses `import` or `#include` |
| `CLPP0801` | The path does not exist |
| `CLPP1001` | The module graph has a cycle |
