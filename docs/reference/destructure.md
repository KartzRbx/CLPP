---
title: "auto [a, b] ="
sidebar_label: "destructure"
---

# auto [a, b] =

<div class="clpp-ref-meta">Concurrency / multiple returns</div>

Unpack multiple return values. The usual pairing with `pcall`.

## Syntax

```clpp
auto [a, b] = expr;
```

## Parameters

None.

## Return value

Each name is a local.

## Luau emit

`local a, b = expr`

## Description

Not structured bindings for structs. Not `auto [x, y]` on a table field unpack unless `expr` returns multiple values.

## Example

```clpp
auto [success, result] = pcall(func []() {
    return DataStore::GetAsync("PlayerData");
});
```

Emits:

```luau
local success, result = pcall(function()
	return DataStore:GetAsync("PlayerData")
end)
```

## See also

[pcall](pcall) · [auto](auto)
