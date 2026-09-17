---
title: "parallel"
sidebar_label: "parallel"
---

# parallel

<div class="clpp-ref-meta">Concurrency</div>

Parallel Luau block. Emits `task.desynchronize` / `task.synchronize`.

## Syntax

```clpp
parallel {
    ComputeComplexPhysics();
};
```

## Parameters

None.

## Return value

None.

## Luau emit

`task.desynchronize() … task.synchronize()`

## Description

Actors and thread safety are Roblox engine rules. Do not touch Instances unsafely inside the block. See Roblox Parallel Luau docs for what is legal.

## Example

```clpp
parallel {
    ComputeComplexPhysics();
};
```

Emits:

```luau
task.desynchronize()
ComputeComplexPhysics()
task.synchronize()
```

## See also

[spawn](spawn) · [async](async)
