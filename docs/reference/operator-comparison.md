---
title: "== != < > <= >="
sidebar_label: "== !="
---

# `== != < > <= >=`

<div class="clpp-ref-meta">Operator</div>

`!=` is the one that changes: it emits Luau `~=`. The others stay the same.

## Syntax

```clpp
a == b
a != b
a < b
```

## Parameters

None.

## Return value

`bool`.

## Luau emit

`==  ~=  <  >  <=  >=`

## Description

There is no `===`. `null` compares with `==` / `!=`.

## Example

```clpp
if (currentValue.Value != newValue) {
    currentValue.Value = newValue;
}
```

Emits:

```luau
if currentValue.Value ~= newValue then
	currentValue.Value = newValue
end
```

## See also

[operator-logic](operator-logic) · [null](null) · [if](if)
