---
title: "#pragma strict"
sidebar_label: "pragma strict"
---

# #pragma strict

<div class="clpp-ref-meta">Preprocessor</div>

Emit `--!strict` at the top of the Luau file.

## Syntax

```clpp
#pragma strict
```

## Parameters

None.

## Return value

None.

## Luau emit

`--!strict`

## Description

Default is **not** strict unless config `"strict": true`. [`#pragma nstrict`](pragma-nstrict) never emits the comment.

## Example

```clpp
#pragma strict
void init() {}
```

Emits:

```luau
--!strict
```

## See also

[pragma-nstrict](pragma-nstrict) · [pragma-once](pragma-once) · [include](include)
