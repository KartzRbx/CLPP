---
title: "null"
sidebar_label: "null"
---

# null

<div class="clpp-ref-meta">Literal</div>

Absence of a value. `nullptr` is accepted as a synonym. Emits Luau `nil`.

## Syntax

```clpp
Player ref = null;
if (player != null) { }
```

## Parameters

None.

## Return value

The `nil` value.

## Luau emit

`nil`

## Description

There is no C `NULL` macro. Compare with [`!=`](operator-comparison). [`guard (player != null)`](guard) is the usual early-out.

Uninitialized locals (`int coins;`) also emit `nil` — always initialize.

## Example

```clpp
Player playerRef = null;
guard (playerRef != null) else {
    return;
}
```

Emits:

```luau
local playerRef: Player = nil
if not (playerRef ~= nil) then
	return
end
```

## See also

[optional](optional) · [guard](guard) · [instance-pointer](instance-pointer)
