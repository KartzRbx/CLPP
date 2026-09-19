---
title: ": (type / protected call)"
sidebar_label: ":"
---

# : (type / protected call)

<div class="clpp-ref-meta">Operator</div>

Two jobs: **types** on names, and **Safe Mode** calls that cannot crash the script.

## Syntax

```clpp
age: int = 10;
void Greet(player: Player) { }

player:Kick();
auto child = workspace:FindFirstChild("x");
```

## Parameters

None.

## Return value

For a call: the result on success, `nil` on error. The script keeps running.

## Luau emit

`local age: number = 10`

```luau
(function()
	local _ok, _r = pcall(function()
		return player:Kick()
	end)
	return if _ok then _r else nil
end)()
```

## Description

**Types.** Prefix C++ types still work (`int age = 10`). `:` is the other spelling: `age: int = 10`, and `name: Type` on parameters. In the emitted Luau, every local already uses `name: Type`.

**Protected calls.** `player:Kick()` is Luau-style `:` **and** a `pcall`. Prefer [`.`](operator-property) when the call should throw (`player.Kick()`). Use `:` when a missing child, a bad argument, or `report` must not stop the listener.

Range-for uses `in` (or `:` in a different position): [`for (T x in list)`](range-for).

Bare `Table:Key` without `()` still emits `Table.Key` (old table-key spelling). New code uses `.`: `DataService.Server`.

## Example

```clpp
age: int = 10;
Instance child = workspace:FindFirstChild("Missing");
guard (child != null) else {
    return;
}
```

Emits:

```luau
local age: number = 10
local child: Instance = (function()
	local _ok, _r = pcall(function()
		return workspace:FindFirstChild("Missing")
	end)
	return if _ok then _r else nil
end)()
if not (child ~= nil) then
	return
end
```

## See also

[pcall](pcall) · [operator-property](operator-property) · [operator-method](operator-method) · [int](int)
