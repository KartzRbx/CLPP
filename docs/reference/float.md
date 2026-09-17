---
title: "float"
sidebar_label: "float"
---

# float

<div class="clpp-ref-meta">Type</div>

Floating-point number. Same Luau emit as `int` and `double`.

## Syntax

```clpp
float name = value;
```

## Parameters

None.

## Return value

A value of type `float`, emitted as `number`.

## Luau emit

`number`

## Description

[`observable float`](observable) becomes `NumberValue`. Use for speed, alpha, damage with fractions.

## Example

```clpp
float speed = 16.5;
```

Emits:

```luau
local speed: number = 16.5
```

## See also

[int](int) · [double](double)
