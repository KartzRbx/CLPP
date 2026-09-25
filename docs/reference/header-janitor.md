---
title: "<clpp/libs/janitor.clh>"
sidebar_label: "janitor.clh"
---

# `<clpp/libs/janitor.clh>`

<div class="clpp-ref-meta">Standard header</div>

IntelliSense **and** `require` of Janitor. Needed for [`~>`](operator-janitor).

## Syntax

```clpp
link @clpp.libs.janitor as Janitor;
```

## Parameters

None.

## Return value

None.

## Luau emit

`require(Janitor)`

## Description

`new Janitor()` emits `Janitor.new()`. `~>` looks up a janitor in scope.

## Example

```clpp
link @clpp.libs.janitor as Janitor;
auto janitor = new Janitor();
```

Emits:

```luau
local Janitor = require(...)
local janitor = Janitor.new()
```

## See also

[operator-janitor](operator-janitor) · [new](new) · [include](include)
