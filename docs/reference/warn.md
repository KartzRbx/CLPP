---
title: "warn"
sidebar_label: "warn"
---

# warn

<div class="clpp-ref-meta">I/O · builtin</div>

Writes a yellow warning. Same name and meaning as Luau `warn`. Use for recoverable problems.

## Syntax

```clpp
void warn(...);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `...` | `any` | Warning payload. |

## Return value

`void`.

## Luau emit

`warn(...)`

## Description

Does not stop the thread. Pair with [`guard`](guard) when the failure is expected and you `return` afterwards. Use [`report`](report) when the program cannot continue.

## Example

```clpp
guard (player != null) else {
    warn("Invalid player");
    return;
}
```

Emits:

```luau
if not (player ~= nil) then
	warn("Invalid player")
	return
end
```

## See also

[post](post) · [report](report) · [guard](guard)
