---
title: "lambda []"
sidebar_label: "[]"
---

# lambda []

<div class="clpp-ref-meta">Functions</div>

Closure. Empty `[]` only — no C++ captures `[x]` / `[&]`. Luau still closes over outer locals.

## Syntax

```clpp
func [](T arg) { }
[]() { }
func cb = []() {};
```

## Parameters

None.

## Return value

A function value.

## Luau emit

`function(arg: T) … end`

## Description

Prefix with [`func`](func) when you want the type on the lambda. Passing a method by name from inside `Class::` binds `self`: `function(...) self:OnPlayer(...) end`.

Keep Instances alive with Janitor; closures do not own Roblox lifetime.

## Example

```clpp
players::PlayerAdded::Connect(func [](Player* playerEntered) {
    post("New player: " .: playerEntered.Name);
});
```

Emits:

```luau
players.PlayerAdded:Connect(function(playerEntered: Player)
	print("New player: " .. playerEntered.Name)
end)
```

## See also

[func](func) · [Connect](Connect) · [function](function) · [this](this)
