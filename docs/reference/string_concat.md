---
title: "string_concat"
sidebar_label: "string_concat"
---

# string_concat

<div class="clpp-ref-meta">Builtin · strings</div>

Variadic string join. Prefer the [`.:`](operator-concat) operator for two operands.

## Syntax

```clpp
string string_concat(...);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `...` | `any` | Values coerced and joined with Luau `..`. |

## Return value

`string`. Zero args → `""`. One arg → that value.

## Luau emit

`(a .. b .. c)`

## Description

Valid even when you mix numbers. Binary concatenation is still [`.:`](operator-concat). Do **not** write Lua `..`, and do **not** use `+` on strings.

## Example

```clpp
return string_concat(player.Name, "_", "LeaderstatsJanitor");
return player.Name .: "_" .: "LeaderstatsJanitor";
```

Emits:

```luau
return (player.Name .. "_" .. "LeaderstatsJanitor")
return (player.Name .. "_") .. "LeaderstatsJanitor"
```

## See also

[operator-concat](operator-concat) · [string](string) · [to_string](to_string)
