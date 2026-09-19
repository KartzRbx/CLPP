---
title: "@this"
sidebar_label: "@this"
---

# @this

<div class="clpp-ref-meta">OOP</div>

The current object inside `Class::Method`. Emits Luau `self`. `@field` is that object's member. `this` (no `@`) is the same alias.

## Syntax

```clpp
void Service::Tick() {
    @janitor.Cleanup();
    @this;
    this.janitor.Add(conn);
    coins = coins + 1; // bare field → self.coins
}
```

## Parameters

None.

## Return value

The receiver table.

## Luau emit

`self` · `self.field`

## Description

Only valid inside [`Class::Method`](class-method). A free function or `void init()` that uses `@this` / `@field` is an error.

`@this` is a value: pass it to other functions (`other.Register(@this)` → `other:Register(self)`). There is no `this->` and no `@this` parameter on the signature — `::` already injects the receiver.

`[]` still indexes. `[[server]]` is still an attribute. `@` here is only the receiver sigil.

## Example

```clpp
void CombatServer::BindPart(BasePart part) {
    @janitor.Add(part, "Destroy");
    other.Register(@this);
}
```

Emits:

```luau
function CombatServer:BindPart(part: BasePart)
	self.janitor:Add(part, "Destroy")
	other:Register(self)
end
```

## See also

[struct](struct) · [class-method](class-method)
