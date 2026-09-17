---
title: "to_number"
sidebar_label: "to_number"
---

# to_number

<div class="clpp-ref-meta">Builtin · conversion</div>

Parse a number from text, or pass a number through. Analog of `std::stod` / Luau `tonumber`.

## Syntax

```clpp
float to_number(value);
float to_number(value, base);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `value` | `any` | Text or number. |
| `base` | `int` | Optional radix for text (2–36). |

## Return value

`float` (Luau number). Parse failure → `null`.

## Luau emit

`tonumber(value)` · `tonumber(value, base)`

## Description

Do **not** write Luau `tonumber` in CL++ source. Guard the result if the text might be invalid.

## Example

```clpp
string raw = "42";
float n = to_number(raw);
guard (n != null) else {
    warn("not a number");
    return;
}
post(n);
```

Emits:

```luau
local raw: string = "42"
local n: number = tonumber(raw)
if not (n ~= nil) then
	warn("not a number")
	return
end
print(n)
```

## See also

[to_string](to_string) · [to_bool](to_bool) · [float](float) · [int](int)
