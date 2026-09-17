---
title: "#pragma optimize"
sidebar_label: "pragma optimize"
---

# #pragma optimize

<div class="clpp-ref-meta">Preprocessor</div>

Emit `--!optimize N`. Bare `optimize` means level **2**.

## Syntax

```clpp
#pragma optimize
#pragma optimize 2
#pragma optimize 1
#pragma optimize 0
```

## Luau emit

```luau
--!optimize 2
```

## Description

`0`, `1`, and `2` are the Luau optimization levels. `#pragma optimize` with no number emits `--!optimize 2`.

## See also

[pragma-native](pragma-native) · [pragma-strict](pragma-strict)
