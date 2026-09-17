---
title: "while"
sidebar_label: "while"
---

# while

<div class="clpp-ref-meta">Control flow</div>

Loop while the condition is true. There is no `do/while`.

## Syntax

```clpp
while (cond) {
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `cond` | `bool` | Checked before each iteration. |

## Return value

None.

## Luau emit

`while … do … end`

## Description

There is no [`continue`](../unsupported). Use nested `if` or restructure. [`break`](break) leaves the loop.

## Example

```clpp
while (true) {
    post("tick");
    break;
}
```

Emits:

```luau
while true do
	print("tick")
	break
end
```

## See also

[for](for) · [range-for](range-for) · [break](break)
