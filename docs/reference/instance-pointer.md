---
title: "Instance types"
sidebar_label: "Instance"
---

# Instance types

<div class="clpp-ref-meta">Type</div>

Write the Roblox class name. `Player player` is an Instance of class Player. There is no address-of, no `->`, and no C pointer type.

## Syntax

```clpp
Player player = null;
player.Name = "Kartz";
player.FindFirstChild("leaderstats");
```

## Parameters

None.

## Return value

The Instance.

## Luau emit

`Player`

## Description

Properties and instance methods use [`.`](operator-property): `player.Name`, `player.FindFirstChild("leaderstats")`, `player.Kick()`. Protected calls use [`:`](operator-table). Static names use [`::`](operator-method) (`task::wait`, `Vector3::new`). Lifetime is Roblox's: `Destroy` or Janitor.

[`match`](match) arms use the same class name: `Part p`.

[`observable`](observable) of a non-primitive becomes `ObjectValue`.

## Example

```clpp
player.Name = "Kartz";
player.FindFirstChild("leaderstats");
```

Emits:

```luau
player.Name = "Kartz"
player:FindFirstChild("leaderstats")
```

## See also

[new](new) · [operator-method](operator-method) · [operator-property](operator-property) · [null](null)
