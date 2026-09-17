---
title: "switch"
sidebar_label: "switch"
---

# switch

<div class="clpp-ref-meta">Control flow</div>

Evaluates the discriminant **once**. `break` leaves the switch. No C fall-through; stacked `case`s share a body.

## Syntax

```clpp
switch (action) {
case "buy":
case "purchase":
    Grant(player);
    break;
default:
    warn("unknown");
    break;
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `action` | `any` | Compared with `==` to each case. |

## Return value

None.

## Luau emit

`repeat … until true  wrapping if / elseif`

## Description

Emitted as `if` / `elseif` / `else` inside `repeat … until true` so [`break`](break) still leaves the switch. Stacked `case`s share the body. `default` is the fallback.

## Example

```clpp
switch (action) {
case "buy":
    Grant(player);
    break;
default:
    warn("unknown");
    break;
}
```

Emits:

```luau
-- discriminant evaluated once, then if/elseif inside repeat-until-true
```

## See also

[match](match) · [if](if) · [break](break)
