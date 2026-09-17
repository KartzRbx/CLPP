---
title: "#pragma nostrict"
sidebar_label: "pragma nostrict"
---

# #pragma nostrict

<div class="clpp-ref-meta">Preprocessor</div>

Emit `--!nonstrict`. Aliases: `nstrict`, `nonstrict`.

## Syntax

```clpp
#pragma nostrict
#pragma nstrict
#pragma nonstrict
```

## Luau emit

```luau
--!nonstrict
```

## Description

Turns off Luau strict mode for this file. Last of `strict` / `nostrict` wins.

## See also

[pragma-strict](pragma-strict) · [pragma-native](pragma-native)
