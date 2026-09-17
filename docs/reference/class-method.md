---
title: "Class::Method"
sidebar_label: "Class::Method"
---

# Class::Method

<div class="clpp-ref-meta">OOP</div>

Method implementation. Emits `function Class:Method(...)`.

## Syntax

```clpp
void Class::Method(T arg) {
    this.field = arg;
}
```

## Parameters

None.

## Return value

Per signature.

## Luau emit

`function Class:Method(arg: T)`

## Description

[`this`](this) is `self`. Bare fields become `self.field`. Untagged files with only `Class::` `return` the table (ModuleScript).

Construct **one** service in [`init()`](init) and use it from lambdas.

## Example

```clpp
void LeaderstatsServer::OnPlayer(Player* player) {
    post(player.Name);
}
```

Emits:

```luau
function LeaderstatsServer:OnPlayer(player: Player)
	print(player.Name)
end
```

## See also

[struct](struct) · [this](this) · [init](init) · [operator-method](operator-method)
