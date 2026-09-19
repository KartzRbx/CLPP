---
title: ":: (static / manual Connect)"
sidebar_label: "::"
---

# :: (static / manual Connect)

<div class="clpp-ref-meta">Operator</div>

CL++ does **not** use `->`. `::` is static scope, datatype/library names, class method definitions, and **manual** signal connections (you Disconnect; Janitor does not).

## Syntax

```clpp
Vector3::new(1, 0, 1);
task::wait(1);
players.PlayerAdded::Connect(fn);
void Class::Method(...) { }
```

## Parameters

None.

## Return value

The call result, or the nested name.

## Luau emit

`Vector3.new(...)` · `task.wait(1)` · `players.PlayerAdded:Connect(fn)` · `function Class:Method`

## Description

**Static / modules:** `task::wait`, `Vector3::new`, `BrickColor::Red()`.

**Manual connections:** `players.PlayerAdded::Connect(fn)` emits `players.PlayerAdded:Connect(fn)` with **no** Janitor. You own `Disconnect`. Prefer [`~>`](operator-janitor) when a janitor is in scope.

**Definitions:** `Class::Method` in a `.clpp` is the method body (`function Class:Method`).

Instance methods on a value use [`.`](operator-property): `player.Kick()`, `workspace.FindFirstChild("x")`. Protected calls use [`:`](operator-table).

## Example

```clpp
task::wait(1);
players.PlayerAdded::Connect(fn);
player.FindFirstChild("x");
DataService.Server.WaitFor(p);
```

Emits:

```luau
task.wait(1)
players.PlayerAdded:Connect(fn)
player:FindFirstChild("x")
DataService.Server:WaitFor(p)
```

## See also

[operator-property](operator-property) · [operator-table](operator-table) · [operator-janitor](operator-janitor) · [class-method](class-method) · [Connect](Connect)
