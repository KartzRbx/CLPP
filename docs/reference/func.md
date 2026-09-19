---
title: "func"
sidebar_label: "func"
---

# func

<div class="clpp-ref-meta">Type</div>

Function type and anonymous callback prefix. Callbacks, listeners, `pcall` bodies.

## Syntax

```clpp
func name = value;
```

## Parameters

None.

## Return value

A value of type `func`, emitted as `(...any) -> any`.

## Luau emit

`(...any) -> any`

## Description

There are no C++ captures. Write `func (int n) { }` or assign `func cb = func () {};`.

## Example

```clpp
func onCoinsChanged = func (int newValue) {
    post("New value: " .: newValue);
};
```

Emits:

```luau
local onCoinsChanged: (...any) -> any = function(newValue: number)
	print("New value: " .. newValue)
end
```

## See also

[lambda](lambda) · [function](function) · [Connect](Connect)
