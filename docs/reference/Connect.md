---
title: "Connect"
sidebar_label: "Connect"
---

# Connect

<div class="clpp-ref-meta">Signals</div>

Subscribe until Disconnect. Prefer `~>Connect` so Janitor owns the connection.

## Syntax

```clpp
name::Connect(fn);
name~>Connect(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `fn` | `func` | Listener. Arguments match the signal payload. |

## Return value

A connection with `:Disconnect()`.

## Luau emit

`name:Connect(fn)  — or janitor:Add(..., "Disconnect")`

## Description

Works on `signal<T>`, RBXScriptSignals (`PlayerAdded`), and anything with `:Connect`.

## Example

```clpp
players.PlayerAdded::Connect(func (Player* player) {
    post(player.Name);
});
```

Emits:

```luau
players.PlayerAdded:Connect(function(player: Player)
	print(player.Name)
end)
```

## See also

[Once](Once) · [operator-janitor](operator-janitor) · [Fire](Fire) · [lambda](lambda)
