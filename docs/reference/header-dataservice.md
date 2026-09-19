---
title: "<clpp/libs/dataservice.clh>"
sidebar_label: "dataservice.clh"
---

# `<clpp/libs/dataservice.clh>`

<div class="clpp-ref-meta">Standard header</div>

DataService table (`.Server` / `.Client`) plus `require`.

## Syntax

```clpp
#include <clpp/libs/dataservice.clh>
```

## Parameters

None.

## Return value

None.

## Luau emit

`require(DataService)`

## Description

Access the singleton with [`.`](operator-property): `DataService.Server.WaitFor(player)`.

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

[operator-table](operator-table) · [operator-designated](operator-designated) · [await](await)
