---
title: "report"
sidebar_label: "report"
---

# report

<div class="clpp-ref-meta">I/O · builtin</div>

Throws. This is CL++'s analog of C++ `throw` / Luau `error`. The current thread stops.

## Syntax

```clpp
void report(...);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `...` | `any` | Error payload (usually a string). |

## Return value

Does not return.

## Luau emit

`error(...)`

## Description

There is no `try/catch` in CL++. Use [`pcall`](pcall) when you need to catch a failure. `report` is for unrecoverable states.

## Example

```clpp
if (coins < 0) {
    report("Balance sync error.");
}
```

Emits:

```luau
if coins < 0 then
	error("Balance sync error.")
end
```

## See also

[post](post) · [warn](warn) · [pcall](pcall)
