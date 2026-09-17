---
title: "&& || !"
sidebar_label: "&& || !"
---

# && || !

<div class="clpp-ref-meta">Operator</div>

Boolean logic. Emits Luau `and` / `or` / `not`.

## Syntax

```clpp
a && b || !c
```

## Parameters

None.

## Return value

`bool` (Luau truthiness).

## Luau emit

`and  or  not`

## Description

There is no ternary `? :`. Use `if` / `else`. Short-circuit matches Luau.

## Example

```clpp
if (player != null && player.Parent) {
    post(player.Name);
}
```

Emits:

```luau
if player ~= nil and player.Parent then
	print(player.Name)
end
```

## See also

[operator-comparison](operator-comparison) · [if](if) · [guard](guard)
