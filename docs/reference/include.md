---
title: "#include"
sidebar_label: "#include"
---

# #include

<div class="clpp-ref-meta">Preprocessor</div>

Angle-bracket includes are IntelliSense (and `require` for libs). Quoted sibling stem is inlined.

## Syntax

```clpp
#include <clpp/roblox.clh>
#include "LeaderstatsServer.clh"
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `path` | `string` | Header path. |

## Return value

None.

## Luau emit

`require(…) for libs / non-stem quotes; stem quote is inlined`

## Description

**Quoted, same stem** as the `.clpp`: the header is **inlined**.

**Quoted, any other name**: `require`.

**Angle** `<clpp/libs/janitor.clh>`: IntelliSense **and** `require` Janitor. `<clpp/roblox.clh>` is IntelliSense only (engine globals).

See [headers](header-roblox).

## Example

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
```

Emits:

```luau
local Janitor = require(...)
```

## See also

[header-roblox](header-roblox) · [header-janitor](header-janitor) · [pragma-strict](pragma-strict)
