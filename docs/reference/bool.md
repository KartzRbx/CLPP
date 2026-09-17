---
title: "bool"
sidebar_label: "bool"
---

# bool

<div class="clpp-ref-meta">Type</div>

Boolean. Literals are `true` and `false`.

## Syntax

```clpp
bool name = value;
```

## Parameters

None.

## Return value

A value of type `bool`, emitted as `boolean`.

## Luau emit

`boolean`

## Description

[`observable bool`](observable) becomes `BoolValue`. `!` emits `not`. There is no `YES`/`NO`.

## Example

```clpp
bool isActive = true;
if (!isActive) {
    return;
}
```

Emits:

```luau
local isActive: boolean = true
if not isActive then
	return
end
```

## See also

[operator-logic](operator-logic) · [observable](observable)
