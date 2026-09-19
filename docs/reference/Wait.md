---
title: "Wait"
sidebar_label: "Wait"
---

# Wait

<div class="clpp-ref-meta">Signals</div>

Yield until the next `Fire` (or next RBXScriptSignal fire).

## Syntax

```clpp
auto payload = name.Wait();
```

## Parameters

None.

## Return value

The next payload (possibly multiple values).

## Luau emit

`name:Wait()`

## Description

Blocks the current thread. Prefer [`Connect`](Connect) for ongoing work. Combine with [`await`](await) only if `Wait` is Promise-like — engine signals yield directly.

## Example

```clpp
OnCoinsUpdated.Wait();
```

Emits:

```luau
OnCoinsUpdated:Wait()
```

## See also

[Fire](Fire) · [Connect](Connect) · [async](async)
