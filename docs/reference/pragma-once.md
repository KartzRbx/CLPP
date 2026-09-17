---
title: "#pragma once"
sidebar_label: "pragma once"
---

# #pragma once

<div class="clpp-ref-meta">Preprocessor</div>

Parsed and ignored. Headers are not multiply included the C++ way.

## Syntax

```clpp
#pragma once
```

## Parameters

None.

## Return value

None.

## Luau emit

`(ignored)`

## Description

Safe to write at the top of `.clh` files for clangd / habit. The compiler does not implement include guards.

## Example

```clpp
#pragma once
struct Data {};
```

Emits:

```luau
-- struct emit only
```

## See also

[include](include) · [pragma-strict](pragma-strict)
