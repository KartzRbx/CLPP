---
title: "signal<T...>"
sidebar_label: "signal"
---

# `signal<T...>`

<div class="clpp-ref-meta">Type</div>

Typed BindableEvent. Declaration emits `__signal()`. Fire with `.Fire`; listen with `~>` or `::Connect`.

## Syntax

```clpp
signal<Player, int> OnCoinsUpdated;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T...` | `types` | Payload types, comma-separated. |

## Return value

A signal object (`RBXScriptSignal`-like).

## Luau emit

`__signal()`

## Description

See the operations: [`Fire`](Fire), [`Connect`](Connect), [`Once`](Once), [`Wait`](Wait). Prefer [`~>`](operator-janitor) so Janitor owns the connection.

## Example

```clpp
signal<Player, int> OnCoinsUpdated;
OnCoinsUpdated.Fire(player, 500);
```

Emits:

```luau
local OnCoinsUpdated = __signal()
OnCoinsUpdated:Fire(player, 500)
```

## See also

[Fire](Fire) · [Connect](Connect) · [Once](Once) · [Wait](Wait) · [operator-janitor](operator-janitor)
