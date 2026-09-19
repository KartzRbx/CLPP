---
title: ".Field = (designated init)"
sidebar_label: ".Field ="
---

# .Field = (designated init)

<div class="clpp-ref-meta">Operator</div>

C++ designated initializer. Becomes a Luau table field.

## Syntax

```clpp
Type {
    .Field = value,
    .Other = value2
}
```

## Parameters

None.

## Return value

A table.

## Luau emit

`{ Field = value, Other = value2 }`

## Description

Used for option bags (`DataServiceOptions`). Not an Instance property write — that is [`obj.Prop =`](operator-property).

## Example

```clpp
DataService.Server.Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true
});
```

Emits:

```luau
DataService.Server:Init({
	Template = playerData,
	StoreName = "PlayerData",
	UseMock = true
})
```

## See also

[operator-property](operator-property) · [struct](struct) · [operator-table](operator-table)
