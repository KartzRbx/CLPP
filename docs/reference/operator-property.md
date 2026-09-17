---
title: ". (property)"
sidebar_label: "."
---

# . (property)

<div class="clpp-ref-meta">Operator</div>

Instance or value property. Stays `.` in Luau.

## Syntax

```clpp
instance.Property
instance.Property = value;
```

## Parameters

None.

## Return value

The property value.

## Luau emit

`instance.Property`

## Description

Use `.` for `Name`, `Parent`, `Value`, `Size`, `CFrame`, … Methods are [`::`](operator-method). Dictionary keys are [`:`](operator-table).

Designated initializers also start with `.`: [`.Field = value`](operator-designated).

## Example

```clpp
player.Name = "Kartz";
part.Size = Vector3(8, 1, 8);
```

Emits:

```luau
player.Name = "Kartz"
part.Size = Vector3.new(8, 1, 8)
```

## See also

[operator-method](operator-method) · [operator-designated](operator-designated) · [GetPropertyChangedSignal](GetPropertyChangedSignal)
