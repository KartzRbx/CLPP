---
title: "auto"
sidebar_label: "auto"
---

# auto

<div class="clpp-ref-meta">Type</div>

Infer the type from the initializer. Prefer explicit types on parameters and struct fields.

## Syntax

```clpp
auto players = GetService<Players>();
auto janitor = new Janitor();
auto [ok, result] = pcall(fn);
```

## Parameters

None.

## Return value

Whatever the initializer produces.

## Luau emit

`local name = …  (Luau annotation when known)`

## Description

Inferred for `new Class(...)`, [`GetService<T>()`](GetService), datatype constructors, and destructuring.

Do not use `auto` as a replacement for a public API type.

## Example

```clpp
auto coins = new IntValue(leaderstats);
```

Emits:

```luau
local coins: IntValue = Instance.new("IntValue")
coins.Parent = leaderstats
```

## See also

[new](new) · [GetService](GetService) · [destructure](destructure)
