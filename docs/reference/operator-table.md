---
title: ": (table key)"
sidebar_label: ":"
---

# : (table key)

<div class="clpp-ref-meta">Operator</div>

Dictionary / module table key. Emits `.`. Not a method call — that is `::`.

## Syntax

```clpp
Table:Key
Table:Key::Method()
```

## Parameters

None.

## Return value

The field.

## Luau emit

`Table.Key`

## Description

`DataService:Server` → `DataService.Server`. Combine with [`::`](operator-method): `DataService:Server::WaitFor(p)`.

Range-for uses `:` in a different position: [`for (T x : list)`](range-for). The compiler distinguishes them.

## Example

```clpp
DataService:Server::Init(opts);
post(stats:Coins);
```

Emits:

```luau
DataService.Server:Init(opts)
print(stats.Coins)
```

## See also

[operator-method](operator-method) · [dictionary](dictionary) · [range-for](range-for)
