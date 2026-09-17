---
title: "vector<T>"
sidebar_label: "vector"
---

# `vector<T>`

<div class="clpp-ref-meta">Collection</div>

Alias of [`array<T>`](array). Not `Vector3`.

## Syntax

```clpp
vector<T> name = { a, b };
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Element type. |

## Return value

A list (`{T}`).

## Luau emit

`{T}`

## Description

`Vector3` is a Roblox datatype (a value). `vector<int>` is a list of numbers. Do not confuse them.

## Example

```clpp
vector<int> scores = {1, 2, 3};
```

Emits:

```luau
local scores: {number} = { 1, 2, 3 }
```

## See also

[array](array) · [dictionary](dictionary)
