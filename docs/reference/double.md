---
title: "double"
sidebar_label: "double"
---

# double

<div class="clpp-ref-meta">Type</div>

Same emit as `float`. Write `double` when you want the C++ name.

## Syntax

```clpp
double name = value;
```

## Parameters

None.

## Return value

A value of type `double`, emitted as `number`.

## Luau emit

`number`

## Description

[`observable double`](observable) becomes `NumberValue`.

## Example

```clpp
double alpha = 0.25;
```

Emits:

```luau
local alpha: number = 0.25
```

## See also

[int](int) · [float](float)
