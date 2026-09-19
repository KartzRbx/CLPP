---
title: "optional<T>"
sidebar_label: "optional"
---

# `optional<T>`

<div class="clpp-ref-meta">Type</div>

A value that may be missing. Emits Luau `T?`.

## Syntax

```clpp
optional<T> name = null;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Inner type. |

## Return value

`T` or [`null`](null).

## Luau emit

`T?`

## Description

Not `std::optional` with `.value()`. Test with `!= null` or [`guard`](guard).

## Example

```clpp
optional<Player> target = null;
```

Emits:

```luau
local target: Player? = nil
```

## See also

[null](null) · [guard](guard)
