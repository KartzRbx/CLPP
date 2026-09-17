---
title: "observable T"
sidebar_label: "observable"
---

# observable T

<div class="clpp-ref-meta">Type</div>

A ValueBase whose identifier reads and writes `.Value`. Assigning fires `Changed`.

## Syntax

```clpp
observable T name = value;
name.OnChange(fn);
name = next;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | `int` → IntValue, `float`/`double` → NumberValue, `string` → StringValue, `bool` → BoolValue, else ObjectValue. |

## Return value

The ValueBase instance.

## Luau emit

`Instance.new("...Value"); name.Value = …`

## Description

Reading `name` in an expression uses `.Value`. Writing `name = x` writes `.Value` and fires `Changed`.

[`.OnChange(fn)`](OnChange) is `Changed:Connect(fn)`.

## Example

```clpp
observable int coins = 100;
coins.OnChange(func [](int newValue) {
    post("now " .: newValue);
});
coins = 50;
post(coins);
```

Emits:

```luau
local coins: IntValue = Instance.new("IntValue")
coins.Value = 100
coins.Changed:Connect(function(newValue: number)
	print("now " .. newValue)
end)
coins.Value = 50
print(coins.Value)
```

## See also

[OnChange](OnChange) · [int](int) · [new](new)
