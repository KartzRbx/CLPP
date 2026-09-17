---
title: "if / else if / else"
sidebar_label: "if"
---

# if / else if / else

<div class="clpp-ref-meta">Control flow</div>

C-style branch. `else if` emits Luau `elseif`.

## Syntax

```clpp
if (cond) {
} else if (other) {
} else {
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `cond` | `bool` | Parentheses required. |

## Return value

None (statements).

## Luau emit

`if / elseif / else / end`

## Description

There is no ternary `? :`. There is no `elif` spelling — write `else if`.

## Example

```clpp
if (coins > 50) {
    post("Enough balance!");
} else if (coins == 0) {
    warn("No coins!");
} else {
    report("Balance sync error.");
}
```

Emits:

```luau
if coins > 50 then
	print("Enough balance!")
elseif coins == 0 then
	warn("No coins!")
else
	error("Balance sync error.")
end
```

## See also

[guard](guard) · [switch](switch) · [match](match) · [operator-logic](operator-logic)
