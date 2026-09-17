---
title: ":: (method / scope)"
sidebar_label: "::"
---

# :: (method / scope)

<div class="clpp-ref-meta">Operator</div>

CL++ does **not** use `->`. `::` is method call and nested name. On a call it emits `:`; without a call it emits `.`.

## Syntax

```clpp
obj::Method(args);
obj::Child::Connect(fn);
Class::Method(...) { }
```

## Parameters

None.

## Return value

The call result, or the nested name.

## Luau emit

`obj:Method(args)  /  obj.Child`

## Description

**Call:** `player::FindFirstChild("x")` → `player:FindFirstChild("x")`.

**No call:** `players::PlayerAdded` → `players.PlayerAdded` (then `::Connect` becomes `:Connect`).

**Datatype / task:** `Vector3::new` / `task::wait` emit `.` (they are not Instance methods).

**Definition:** `Class::Method` in a `.clpp` is a method implementation (`function Class:Method`).

## Notes

Table keys use [`:`](operator-table). Properties use [`.`](operator-property).

## Example

```clpp
players::PlayerAdded::Connect(fn);
player::FindFirstChild("x");
DataService:Server::WaitFor(p);
```

Emits:

```luau
players.PlayerAdded:Connect(fn)
player:FindFirstChild("x")
DataService.Server:WaitFor(p)
```

## See also

[operator-table](operator-table) · [operator-property](operator-property) · [class-method](class-method) · [Connect](Connect)
