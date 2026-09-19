---
title: "to_bool"
sidebar_label: "to_bool"
---

# to_bool

<div class="clpp-ref-meta">Builtin · conversion</div>

Coerce a value to `true` or `false`.

## Syntax

```clpp
bool to_bool(value);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `value` | `any` | Value to coerce. |

## Return value

`bool`

## Luau emit

`not not (value)`

## Description

`null`, `false`, and `0` are falsey in Luau; everything else is truthy. Prefer a real `bool` when you can.

## Example

```clpp
Instance child = folder.FindFirstChild("x");
bool exists = to_bool(child);
```

Emits:

```luau
local child: Instance = folder:FindFirstChild("x")
local exists: boolean = not not (child)
```

## See also

[to_string](to_string) · [to_number](to_number) · [bool](bool) · [null](null)
