---
title: "new"
sidebar_label: "new"
---

# new

<div class="clpp-ref-meta">Builtin · constructor</div>

Constructs a Roblox Instance or a library object. Not C++ heap allocation — there is no `delete`.

## Syntax

```clpp
auto child = new Class(parent);
auto janitor = new Janitor();
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `Class` | `identifier` | Instance class or lib type (`Janitor`). |
| `parent` | `Instance?` | First argument becomes `.Parent` on Instances. |

## Return value

The constructed object.

## Luau emit

`Instance.new("Class") / Class.new()`

## Description

**Instances** (`Folder`, `IntValue`, `Part`, …): emit `Instance.new("Class")`. The first argument is assigned to `.Parent`.

**Libraries** (`Janitor` and similar): emit `Janitor.new()`.

**Datatypes** (`Vector3`, `CFrame`, `UDim2`, `Color3`): do **not** use `new`. Call them as values: `Vector3(8, 1, 8)`.

## Example

```clpp
auto coins = new IntValue(leaderstats);
auto janitor = new Janitor();
part.Size = Vector3(8, 1, 8);
```

Emits:

```luau
local coins: IntValue = Instance.new("IntValue")
coins.Parent = leaderstats
local janitor = Janitor.new()
part.Size = Vector3.new(8, 1, 8)
```

## See also

[GetService](GetService) · [instance-pointer](instance-pointer) · [observable](observable)
