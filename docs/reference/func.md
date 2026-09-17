---
title: "func"
sidebar_label: "func"
---

# func

<div class="clpp-ref-meta">Type</div>

Function type and optional lambda prefix. Callbacks, listeners, `pcall` bodies.

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

Lambdas: empty `[]` (no C++ captures). You may write `func [](int n) { }` or assign `func cb = []() {};`.

## Example

```clpp
func onCoinsChanged = [](int newValue) {
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
