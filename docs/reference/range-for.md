---
title: "for (T x : list)"
sidebar_label: "for-each"
---

# for (T x : list)

<div class="clpp-ref-meta">Control flow</div>

Range-for. Emits `for _, x in list`. The `:` here is not a table key.

## Syntax

```clpp
for (T x : list) {
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Element type. |
| `x` | `ident` | Loop variable. |
| `list` | `iterable` | Array or iterator-producing call. |

## Return value

None.

## Luau emit

`for _, x in list do`

## Description

The index is discarded (`_`). To get keys, iterate a dictionary in Luau style via a helper, or use C-for on numeric arrays.

Parentheses and braces are required.

## Example

```clpp
for (Player* player : players::GetPlayers()) {
    post("Player connected: " .: player.Name);
}
```

Emits:

```luau
for _, player in players:GetPlayers() do
	print("Player connected: " .. player.Name)
end
```

## See also

[for](for) · [array](array) · [operator-table](operator-table)
