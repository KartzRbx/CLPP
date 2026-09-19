---
title: "array<T>"
sidebar_label: "array"
---

# `array<T>`

<div class="clpp-ref-meta">Collection</div>

Ordered list. Also spelled `vector<T>`, `LuaArray<T>`, `span<T>`. Emits a Luau array table `{T}`.

## Syntax

```clpp
array<T> name = { a, b, c };
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Element type. |

## Return value

A list.

## Luau emit

`{T}`

## Description

Brace lists are arrays. Iterate with [range-for](range-for). There is no `std::vector::push_back` — use Luau table APIs (`table.insert`) via `::` if you wrap them, or index.

`map<K,V>` is **not** this — see [`dictionary`](dictionary).

## Example

```clpp
array<string> names = {"Kartz", "Player1"};
for (string n in names) {
    post(n);
}
```

Emits:

```luau
local names: {string} = { "Kartz", "Player1" }
for _, n in names do
	print(n)
end
```

## See also

[dictionary](dictionary) · [range-for](range-for) · [vector](vector)
