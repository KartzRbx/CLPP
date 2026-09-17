---
title: "guard"
sidebar_label: "guard"
---

# guard

<div class="clpp-ref-meta">Control flow</div>

If the condition is false, the `else` block runs (usually `return`). Early-out, not an `if` replacement.

## Syntax

```clpp
guard (condition) else {
    warn("…");
    return;
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `condition` | `bool` | Must be true to continue. |

## Return value

None. The else block typically returns.

## Luau emit

`if not (condition) then … end`

## Description

The `else` is **required**. After a successful guard, the compiler (and the reader) treat the condition as established — e.g. `player != null`.

## Example

```clpp
guard (player != null) else {
    warn("Invalid player");
    return;
}
post(player.Name);
```

Emits:

```luau
if not (player ~= nil) then
	warn("Invalid player")
	return
end
print(player.Name)
```

## See also

[if](if) · [null](null) · [match](match)
