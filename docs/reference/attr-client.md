---
title: "[[client]]"
sidebar_label: "[[client]]"
---

# [[client]]

<div class="clpp-ref-meta">Attribute</div>

Function exists on the client only.

## Syntax

```clpp
[[client]]
void UpdateUI() { }
```

## Parameters

None.

## Return value

Per function.

## Luau emit

`omitted on server files; RunService:IsClient() wrapper in modules`

## Description

Mirror of [`[[server]]`](attr-server).

## Example

```clpp
[[client]]
void UpdateUI() {}
```

Emits:

```luau
-- emitted only when the file is a LocalScript / IsClient()
```

## See also

[attr-server](attr-server) · [../files](../files)
