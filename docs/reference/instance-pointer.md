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
player::FindFirstChild("leaderstats");
```

## Parameters

None.

## Return value

The Instance.

## Luau emit

`Player  (star stripped)`

## Description

Properties use [`.`](operator-property). Methods use [`::`](operator-method). Lifetime is Roblox's: `Destroy` or Janitor.

[`observable`](observable) of a non-primitive becomes `ObjectValue`.

## Example

```clpp
player.Name = "Kartz";
player::FindFirstChild("leaderstats");
```

Emits:

```luau
player.Name = "Kartz"
player:FindFirstChild("leaderstats")
```

## See also

[new](new) · [operator-method](operator-method) · [operator-property](operator-property) · [null](null)
