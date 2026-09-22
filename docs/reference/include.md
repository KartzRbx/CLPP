---
title: "#include (legacy / host)"
sidebar_label: "#include"
---

# #include (legacy / host)

<div class="clpp-ref-meta">Preprocessor · legacy</div>

**For language modules, use [`import`](import).** This page documents what still exists for Cluaupp headers and old header/impl pairs.

## Prefer

```clpp
import { Wallet } from "./PlayerData.clh";
```

## Legacy / host syntax

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>
#include "LeaderstatsServer.clh"
```

| Form | Effect |
| --- | --- |
| `"Stem.clh"` same stem as the `.clpp` | Text **splice** (header/impl) |
| `"Other.clh"` | `require` |
| `<clpp/libs/…>` | IntelliSense **and** `require` |
| `<clpp/roblox.clh>` | IntelliSense only (engine globals) |

## See also

[import](import) · [Modules](../modules) · [Files](../files)
