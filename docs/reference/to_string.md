---
title: "to_string"
sidebar_label: "to_string"
---

# to_string

<div class="clpp-ref-meta">Builtin · conversion</div>

Convert any value to text. Analog of C++ `std::to_string` and Luau `tostring`.

## Syntax

```clpp
string to_string(value);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `value` | `any` | Value to convert. |

## Return value

`string`

## Luau emit

`tostring(value)`

## Description

Use this when you need an explicit string — for example before storing in a `StringValue`, or when `.:` is not enough. Do **not** write Luau `tostring` in CL++ source.

Template strings already stringify interpolations: `` `coins {n}` ``.

## Example

```clpp
int coins = 50;
string label = to_string(coins);
post("held " .: label);
```

Emits:

```luau
local coins: number = 50
local label: string = tostring(coins)
print("held " .. label)
```

## See also

[to_number](to_number) · [to_bool](to_bool) · [operator-concat](operator-concat) · [string](string)
