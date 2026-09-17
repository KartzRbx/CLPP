---
title: "+ − * /"
sidebar_label: "+ − * /"
---

# + − * /

<div class="clpp-ref-meta">Operator</div>

Numeric arithmetic. `+` does **not** concatenate. `*` is multiply, never pointer deref.

## Syntax

```clpp
a + b - c * d / e
```

## Parameters

None.

## Return value

A number.

## Luau emit

`same operators`

## Description

Left-to-right association with usual precedence. Unary `-` is negation. Join text with [`.:`](operator-concat).

## Example

```clpp
int total = coins + gems * 2;
```

Emits:

```luau
local total: number = coins + gems * 2
```

## See also

[operator-assignment](operator-assignment) · [operator-increment](operator-increment) · [operator-concat](operator-concat)
