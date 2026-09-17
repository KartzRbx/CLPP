---
title: "Fire"
sidebar_label: "Fire"
---

# Fire

<div class="clpp-ref-meta">Signals</div>

Send a payload to every current listener of a `signal`.

## Syntax

```clpp
name::Fire(...);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `...` | `T...` | Must match `signal<T...>`. |

## Return value

`void`.

## Luau emit

`name:Fire(...)`

## Description

This is **send**. Listening is [`Connect`](Connect) / [`Once`](Once) / [`~>`](operator-janitor).

## Example

```clpp
signal<Player*, int> OnCoinsUpdated;
OnCoinsUpdated::Fire(player, 500);
```

Emits:

```luau
local OnCoinsUpdated = __signal()
OnCoinsUpdated:Fire(player, 500)
```

## See also

[signal-type](signal-type) · [Connect](Connect) · [Once](Once) · [Wait](Wait)
