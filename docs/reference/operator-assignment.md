---
title: "= += −= *= /="
sidebar_label: "="
---

# = += −= *= /=

<div class="clpp-ref-meta">Operator</div>

Assignment. Compound forms emit the same operators.

## Syntax

```clpp
name = value;
name += 1;
```

## Parameters

None.

## Return value

The assigned value (statement form).

## Luau emit

`=  +=  -=  *=  /=`

## Description

[`observable`](observable) assignment writes `.Value`. There is no copy-assignment operator overload.

## Example

```clpp
coins += 10;
name = player.Name;
```

Emits:

```luau
coins += 10
name = player.Name
```

## See also

[operator-increment](operator-increment) · [observable](observable)
