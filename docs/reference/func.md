---
title: "func"
sidebar_label: "func"
---

# func

<div class="clpp-ref-meta">Type</div>

Loose function type and anonymous callback prefix. **Loses** parameter and return detail in emit today.

## Syntax

```clpp
func name = value;
func (Player player) { }
```

## Luau emit

`(...any) -> any`

## Description

There are no C++ captures. Prefer **named functions** with typed parameters when you care about checking:

```clpp
void OnCoins(int newValue) {
    post("New value: " .: newValue);
}
```

Checked generics and `TypeId` function types are expanding; bare `func` remains an escape hatch. See [Type system](../architecture/TYPE_SYSTEM).

## Example

```clpp
func onCoinsChanged = func (int newValue) {
    post("New value: " .: newValue);
};
```

## See also

[function](function) · [lambda](lambda) · [Types](../types)
