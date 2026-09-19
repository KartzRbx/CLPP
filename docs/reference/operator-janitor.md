---
title: "~> (janitor Connect / Once)"
sidebar_label: "~>"
---

# `~> (janitor Connect / Once)`

<div class="clpp-ref-meta">Operator</div>

Subscribe and give the connection to Janitor. `~>Connect` and `~>Once` only.

## Syntax

```clpp
signal~>Connect(fn);
signal~>Once(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `fn` | `func` | Listener. |

## Return value

The connection (also stored on the janitor).

## Luau emit

`janitor:Add(signal:Connect(fn), "Disconnect")`

## Description

Looks up `janitor` local, `self.janitor`, or a synthetic `__janitor`. In [`void init()`](init), `__janitor` also gets `game:BindToClose`.

Bare `::Connect` does **not** register with Janitor. Prefer `~>` in production.

`Once` disconnects after the first emission.

## Example

```clpp
players.PlayerAdded~>Connect(func (Player* player) {
    post("Connected and managed automatically!");
});
players.PlayerAdded~>Once(func (Player* player) {
    post("First player only");
});
```

Emits:

```luau
janitor:Add(players.PlayerAdded:Connect(function(player: Player)
	print("Connected and managed automatically!")
end), "Disconnect")
janitor:Add(players.PlayerAdded:Once(function(player: Player)
	print("First player only")
end), "Disconnect")
```

## See also

[Connect](Connect) · [Once](Once) · [Fire](Fire) · [init](init) · [header-janitor](header-janitor)
