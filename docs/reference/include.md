---
title: "#include (removed)"
sidebar_label: "#include (removed)"
---

# #include

<div class="clpp-ref-meta">Removed</div>

`#include` is not a module form. The parser reports `` use `link` ``.

```clpp
link @clpp.roblox;
link @clpp.libs.janitor as Janitor;
link "./LeaderstatsServer.clh" as LeaderstatsServer;
```

`#pragma once`, `#pragma strict`, `#pragma native`, and `#pragma optimize` stay. See [link](import).
