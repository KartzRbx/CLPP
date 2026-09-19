---
title: "for (T x in list)"
sidebar_label: "for-each"
---

# for (T x in list)

<div class="clpp-ref-meta">Control flow</div>

Range-for. `in` names the collection each value comes from. `:` is the same form.

## Syntax

```clpp
for (T x in list) {
}

for (T x : list) {
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Element type. |
| `x` | `ident` | Loop variable. |
| `list` | `iterable` | Array or iterator-producing call. |

## Description

Prefer `in` — it reads as “each `x` from `list`”. The C++-style `:` still works. Parentheses and braces are required.

This is not a protected call and not a C-style `for` header. Prefer `in`. The C++-style `:` in `for (T x : xs)` is the same loop.

## Example

```clpp
for (Player* player in players.GetPlayers()) {
    post("Player connected: " .: player.Name);
}
```

## See also

[for](for) · [array](array) · [operator-table](operator-table)
