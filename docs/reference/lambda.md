---
title: "func (...)"
sidebar_label: "func (...)"
---

# func (...)

<div class="clpp-ref-meta">Functions</div>

Anonymous callback. There is no C++ capture list — CL++ has no pointers to capture. Luau still closes over outer locals.

## Syntax

```clpp
func (T arg) { }
func () { }
func cb = func () {};
```

## Parameters

None.

## Return value

A function value.

## Luau emit

`function(arg: T) … end`

## Description

[`func`](func) is both the type and the keyword that starts an inline callback. Passing a method by name from inside `Class::` binds `self`: `function(...) self:OnPlayer(...) end`.

Keep Instances alive with Janitor; closures do not own Roblox lifetime. Do **not** write `func [](…)` or `[]() { }`.

## Example

```clpp
players.PlayerAdded~>Connect(func (Player* playerEntered) {
    post("New player: " .: playerEntered.Name);
});
```

Emits:

```luau
janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	print("New player: " .. playerEntered.Name)
end), "Disconnect")
```

## See also

[func](func) · [Connect](Connect) · [function](function) · [this](this)
