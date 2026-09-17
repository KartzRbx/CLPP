---
title: "this"
sidebar_label: "this"
---

# this

<div class="clpp-ref-meta">OOP</div>

The current object inside `Class::Method`. Emits Luau `self`.

## Syntax

```clpp
void Service::Tick() {
    this.janitor::Cleanup();
    coins = coins + 1; // bare field → self.coins
}
```

## Parameters

None.

## Return value

The receiver.

## Luau emit

`self`

## Description

Bare field names become `self.field`. Calls to other methods become `self:Method(...)`. Parameters and locals shadow fields.

There is no `this->`. Use `this.field` or a bare name.

## Example

```clpp
void LeaderstatsServer::OnPlayer(Player* player) {
    this.janitor::Add(conn);
}
```

Emits:

```luau
function LeaderstatsServer:OnPlayer(player: Player)
	self.janitor:Add(conn)
end
```

## See also

[struct](struct) · [class-method](class-method)
