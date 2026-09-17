---
title: "#pragma nstrict"
sidebar_label: "pragma nstrict"
---

# #pragma nstrict

<div class="clpp-ref-meta">Preprocessor</div>

Never emit `--!strict`, even if the project config asks for it.

## Syntax

```clpp
#pragma nstrict
```

## Parameters

None.

## Return value

None.

## Luau emit

`(no --!strict)`

## Description

Use on files that still need Looser Luau.

## Example

```clpp
#pragma nstrict
```

Emits:

```luau
-- (no directive)
```

## See also

[pragma-strict](pragma-strict)
