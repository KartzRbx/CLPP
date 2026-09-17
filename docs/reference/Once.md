---
title: "Once"
sidebar_label: "Once"
---

# Once

<div class="clpp-ref-meta">Signals</div>

Subscribe for a **single** emission, then disconnect. `~>Once` is janitor-managed.

## Syntax

```clpp
name::Once(fn);
name~>Once(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `fn` | `func` | Listener. |

## Return value

A connection.

## Luau emit

`name:Once(fn)`

## Description

Use for “first player only”, one-shot setup, or handshake events.

## Example

```clpp
players::PlayerAdded~>Once(func [](Player* player) {
    post("First player only");
});
```

Emits:

```luau
janitor:Add(players.PlayerAdded:Once(function(player: Player)
	print("First player only")
end), "Disconnect")
```

## See also

[Connect](Connect) · [operator-janitor](operator-janitor) · [Fire](Fire)
