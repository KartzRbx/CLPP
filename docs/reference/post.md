---
title: "post"
sidebar_label: "post"
---

# post

<div class="clpp-ref-meta">I/O · builtin</div>

Writes values to the output log. This is CL++'s standard print — the analog of C `printf` / C++ `std::cout` / Luau `print`.

## Syntax

```clpp
void post(...);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `...` | `any` | Values to print, separated by commas. |

## Return value

`void`. Nothing is returned.

## Luau emit

`print(...)`

## Description

Every argument is forwarded to Luau `print`. There is no format string. Concatenate text with [`.:`](operator-concat) before printing, or pass several arguments.

Prefer `post` over legacy [`cout`](cout).

## Example

```clpp
post("hello");
post("coins:", 100);
post("online: " .: players.GetPlayers());
```

Emits:

```luau
print("hello")
print("coins:", 100)
print("online: " .. players:GetPlayers())
```

## See also

[warn](warn) · [report](report) · [cout](cout) · [operator-concat](operator-concat)
