---
title: "return"
sidebar_label: "return"
---

# return

<div class="clpp-ref-meta">Control flow</div>

Leave the current function, optionally with a value.

## Syntax

```clpp
return;
return expr;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `expr` | `T` | Must match the function return type when present. |

## Return value

Ends the function.

## Luau emit

`return  /  return expr`

## Description

[`void`](void) functions use `return;`. Multiple return values use [destructuring](destructure) on the caller side, not `return a, b` in current CL++ style — return a table or a single value.

## Example

```clpp
const int DoubleCoins(int coins) {
    return coins;
}
```

Emits:

```luau
const function DoubleCoins(coins: number): number
	return coins
end
```

## See also

[function](function) · [void](void) · [guard](guard)
