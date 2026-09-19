---
title: "OnChange"
sidebar_label: "OnChange"
---

# OnChange

<div class="clpp-ref-meta">Observables</div>

Listen to an `observable`'s `Changed`. Argument is the new `.Value`.

## Syntax

```clpp
name.OnChange(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `fn` | `func` | `func (T newValue) { }`. |

## Return value

A connection.

## Luau emit

`name.Changed:Connect(fn)`

## Description

Only for [`observable T`](observable). For arbitrary Instance properties use [`GetPropertyChangedSignal`](GetPropertyChangedSignal).

## Example

```clpp
observable int coins = 100;
coins.OnChange(func (int newValue) {
    post("now " .: newValue);
});
```

Emits:

```luau
coins.Changed:Connect(function(newValue: number)
	print("now " .. newValue)
end)
```

## See also

[observable](observable) · [GetPropertyChangedSignal](GetPropertyChangedSignal)
