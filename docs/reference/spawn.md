---
title: "spawn"
sidebar_label: "spawn"
---

# spawn

<div class="clpp-ref-meta">Concurrency</div>

Run a block on a new thread. Emits `task.spawn`.

## Syntax

```clpp
spawn {
    task::wait(2);
    post("Delay finished!");
};
```

## Parameters

None.

## Return value

None (fires and forgets).

## Luau emit

`task.spawn(function() … end)`

## Description

The block is a body, not a callback you pass. For Parallel Luau, see [`parallel`](parallel).

## Example

```clpp
spawn {
    task::wait(2);
    post("Delay finished!");
};
```

Emits:

```luau
task.spawn(function()
	task.wait(2)
	print("Delay finished!")
end)
```

## See also

[parallel](parallel) · [await](await) · [async](async)
