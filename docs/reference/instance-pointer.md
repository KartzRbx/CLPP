---
title: "T* (Instance)"
sidebar_label: "T*"
---

# T* (Instance)

<div class="clpp-ref-meta">Type</div>

`Player*` means an Instance of class Player — not a heap pointer. There is no `delete`, `*p`, `&p`, `int&`, or `->`.

## Syntax

```clpp
Player* player = null;
player.Name = "Kartz";
player.FindFirstChild("leaderstats");
```

## Parameters

None.

## Return value

The Instance.

## Luau emit

`Player  (star stripped)`

## Description

Properties and instance methods use [`.`](operator-property): `player.Name`, `player.FindFirstChild("leaderstats")`, `player.Kick()`. Protected calls use [`:`](operator-table). Static names use [`::`](operator-method) (`task::wait`, `Vector3::new`). Lifetime is Roblox's: `Destroy` or Janitor.

[`match`](match) arms write the class name (`Part p`), not a pointer (`Part* p`).

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
