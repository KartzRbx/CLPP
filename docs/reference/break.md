---
title: "break"
sidebar_label: "break"
---

# break

<div class="clpp-ref-meta">Control flow</div>

Leaves the innermost loop or `switch`. There is no `continue`.

## Syntax

```clpp
break;
```

## Parameters

None.

## Return value

None.

## Luau emit

`break`

## Description

In [`switch`](switch), `break` leaves the synthetic `repeat-until-true`. There is no labeled break and no `goto`.

## Example

```clpp
while (true) {
    break;
}
```

Emits:

```luau
while true do
	break
end
```

## See also

[while](while) · [for](for) · [switch](switch) · [../unsupported](../unsupported)
