---
title: "await"
sidebar_label: "await"
---

# await

<div class="clpp-ref-meta">Concurrency</div>

Wait for a Promise-like value inside an `async` function.

## Syntax

```clpp
T x = await expr;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `expr` | `any` | Promise (`:expect()`) or already-resolved value. |

## Return value

The resolved value.

## Luau emit

`__await(expr)`

## Description

Not JS `await` in the event loop sense beyond what Luau Promises provide. Use [`spawn`](spawn) to run a block without blocking the caller.

## Example

```clpp
Data* data = await DataService.Server.WaitFor(player);
```

Emits:

```luau
local data: Data = __await(DataService.Server:WaitFor(player))
```

## See also

[async](async) · [spawn](spawn) · [pcall](pcall)
