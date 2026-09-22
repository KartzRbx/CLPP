---
title: "import"
sidebar_label: "import"
---

# import

<div class="clpp-ref-meta">Modules</div>

Named import of another CL++ module’s exports. Canonical module surface ([RFC 0003](https://github.com/KartzRbx/CLPP/blob/main/rfc/0003-module-system.md)).

## Syntax

```clpp
import { Name } from "./path.clh";
import { Name as Alias } from "./path.clh";
import { A, B } from "./path.clp";
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `Name` | ident | Exported symbol in the module |
| `Alias` | ident | Local name in the consumer (`as`) |
| `path` | string | Module path relative to this file |

## Return value

None. Symbols are bound into the file scope for checking and completion.

## Luau emit

`require(…)` for the module path (host maps the string).

## Description

- Merges only listed names (and their members).
- Cycles are cut with a visit set.
- Missing path → `CLPP0801`.
- Not a text splice — that remains the same-stem `#include` legacy case.

## Example

```clpp
import { Wallet } from "./PlayerData.clh";

void F(Wallet w) {
    post(w.Coins);
}
```

## See also

[Modules guide](../modules) · [legacy #include](include) · [Files](../files)
