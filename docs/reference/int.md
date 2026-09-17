---
title: "int"
sidebar_label: "int"
---

# int

<div class="clpp-ref-meta">Type</div>

32-bit-looking integer in source. Luau numbers are IEEE-754 doubles — `int` is a documentation type.

## Syntax

```clpp
int name = value;
```

## Parameters

None.

## Return value

A value of type `int`, emitted as `number`.

## Luau emit

`number`

## Description

Use for counts, coins, user ids stored as numbers. [`observable int`](observable) becomes `IntValue`.

There is no `int8_t` / `size_t` distinct emit — they are not special.

## Example

```clpp
int coins = 100;
coins += 1;
```

Emits:

```luau
local coins: number = 100
coins += 1
```

## See also

[float](float) · [double](double) · [observable](observable)
