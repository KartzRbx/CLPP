---
title: "#pragma once"
sidebar_label: "pragma once"
---

# #pragma once

<div class="clpp-ref-meta">Preprocessor</div>

Include guard. Safe at the top of every `.clh`. The compiler strips the line before parse, so it does not error.

## Syntax

```clpp
#pragma once
```

## Luau emit

`(none)`

## Description

A header included more than once is skipped. Write this on every header.

## Example

```clpp
#pragma once

struct PlayerData {
    int Coins = 0;
};
```

## See also

[include](include) · [pragma-strict](pragma-strict)
