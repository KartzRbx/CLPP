---
title: ". (property / instance method)"
sidebar_label: "."
---

# . (property / instance method)

<div class="clpp-ref-meta">Operator</div>

The default accessor. Properties stay `.` in Luau. Instance method calls emit Luau `:`.

## Syntax

```clpp
instance.Property
instance.Property = value;
instance.Method(args);
```

## Parameters

None.

## Return value

The property value, or the method result.

## Luau emit

`instance.Property` · `instance:Method(args)`

## Description

Use `.` for **everything standard**: `Name`, `Parent`, `Value`, dictionary keys, and instance methods (`Kick`, `FindFirstChild`, `WaitForChild`, …).

Protected (non-throwing) calls use [`:`](operator-table). Static names and manual `Connect` use [`::`](operator-method). Janitor connections use [`~>`](operator-janitor).

Designated initializers also start with `.`: [`.Field = value`](operator-designated).

## Example

```clpp
player.Name = "Kartz";
player.Kick();
workspace.FindFirstChild("Baseplate");
DataService.Server.WaitFor(player);
```

Emits:

```luau
player.Name = "Kartz"
player:Kick()
workspace:FindFirstChild("Baseplate")
DataService.Server:WaitFor(player)
```

## See also

[operator-table](operator-table) · [operator-method](operator-method) · [operator-janitor](operator-janitor) · [operator-designated](operator-designated)
