---
title: "[[server]]"
sidebar_label: "[[server]]"
---

# [[server]]

<div class="clpp-ref-meta">Attribute</div>

Function exists on the server only.

## Syntax

```clpp
[[server]]
void SaveData(Player player) { }
```

## Parameters

None.

## Return value

Per function.

## Luau emit

`omitted on client files; RunService:IsServer() wrapper in modules`

## Description

On a `.client.clpp`, the function is omitted. On a `.server.clpp`, it emits normally. In a module, emit wraps `RunService:IsServer()`.

## Example

```clpp
[[server]]
void SaveData(Player player) {}
```

Emits:

```luau
-- emitted only when the file is a server Script / IsServer()
```

## See also

[attr-client](attr-client) · [../files](../files)
