---
title: "match"
sidebar_label: "match"
---

# match

<div class="clpp-ref-meta">Control flow</div>

Type switch. Instance types use `IsA`; primitives use `typeof`. `_` is the fallback.

## Syntax

```clpp
match (value) {
    Type name => statement,
    Other o => statement,
    _ => statement
};
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `value` | `any` | Scrutinee. Evaluated once. |

## Return value

None (statement).

## Luau emit

`if x:IsA("Type") then … elseif typeof(x) == "…" then …`

## Description

Arms bind a name (`Part* p`) for the narrowed value. `_` is required if the match is not exhaustive in practice — always provide it.

This is not C++ `std::variant` visit and not Luau `if-then-else` expressions.

## Example

```clpp
match (instance) {
    Part* p => p.BrickColor = BrickColor::Red(),
    Model* m => m.PrimaryPart.BrickColor = BrickColor::Blue(),
    _ => warn("Instance not supported")
};
```

Emits:

```luau
if instance:IsA("Part") then
	local p = instance
	p.BrickColor = BrickColor.Red()
elseif instance:IsA("Model") then
	local m = instance
	m.PrimaryPart.BrickColor = BrickColor.Blue()
else
	warn("Instance not supported")
end
```

## See also

[switch](switch) · [guard](guard) · [static_cast](static_cast) · [instance-pointer](instance-pointer)
