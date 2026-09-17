---
title: "GetPropertyChangedSignal"
sidebar_label: "GetPropertyChangedSignal"
---

# GetPropertyChangedSignal

<div class="clpp-ref-meta">Signals · Instance</div>

Engine signal for one Instance property. Listen with `~>` or `::Connect`.

## Syntax

```clpp
obj::GetPropertyChangedSignal("Name")~>Connect(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `name` | `string` | Property name, e.g. `"Transparency"`. |

## Return value

An RBXScriptSignal.

## Luau emit

`obj:GetPropertyChangedSignal("Name")`

## Description

The callback receives no property value on some engine signals — read `obj.Property` inside the listener.

## Example

```clpp
part::GetPropertyChangedSignal("Transparency")~>Connect(func []() {
    post(part.Transparency);
});
```

Emits:

```luau
part:GetPropertyChangedSignal("Transparency"):Connect(function()
	print(part.Transparency)
end)
```

## See also

[Connect](Connect) · [OnChange](OnChange) · [operator-property](operator-property)
