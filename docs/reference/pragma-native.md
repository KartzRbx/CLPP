---
title: "#pragma native"
sidebar_label: "pragma native"
---

# #pragma native

<div class="clpp-ref-meta">Preprocessor</div>

Emit `--!native` so Luau can compile the script as native code.

## Syntax

```clpp
#pragma native
```

## Luau emit

```luau
--!native
```

## Description

Pairs well with [`#pragma optimize 2`](pragma-optimize). Valid in `.clh`, `.clp`, and `.clpp`.

## Example

```clpp
#pragma native
#pragma optimize 2

void init() {}
```

## See also

[pragma-optimize](pragma-optimize) · [pragma-strict](pragma-strict)
