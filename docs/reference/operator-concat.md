---
title: ".: (concat)"
sidebar_label: ".:"
---

# .: (concat)

<div class="clpp-ref-meta">Operator</div>

String concatenation. Emits Luau `..`. Never use `+` or Lua `..` in CL++.

## Syntax

```clpp
a .: b .: c
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `a` | `string-ish` | Left operand. |
| `b` | `string-ish` | Right operand. |

## Return value

A string.

## Luau emit

`a .. b`

## Description

Associates left-to-right: `a .: b .: c` → `(a .. b) .. c`. [`string_concat`](string_concat) joins many arguments in one `(..)` group.

`+` only adds numbers.

## Example

```clpp
return player.Name .: "_LeaderstatsJanitor";
return player.Name .: "_" .: "LeaderstatsJanitor";
```

Emits:

```luau
return player.Name .. "_LeaderstatsJanitor"
return (player.Name .. "_") .. "LeaderstatsJanitor"
```

## See also

[string_concat](string_concat) · [string](string) · [post](post)
