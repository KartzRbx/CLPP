---
title: "dictionary<K, V>"
sidebar_label: "dictionary"
---

# `dictionary<K, V>`

<div class="clpp-ref-meta">Collection</div>

String-keyed (or typed-key) map. `map<K,V>` is the same emit. Access keys with [`.`](operator-property).

## Syntax

```clpp
dictionary<K, V> name = {
    {"Key", value},
    {"Other", value2}
};
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `K` | `type` | Key type (usually `string`). |
| `V` | `type` | Value type. |

## Return value

A map table.

## Luau emit

`{ [K]: V }`

## Description

Initializer entries are `{"Key", value}` pairs, emitted as `Key = value` when the key is a string.

Read/write: `stats.Coins` → `stats.Coins`. Instance properties use the same `.`.

## Example

```clpp
dictionary<string, int> stats = {
    {"Coins", 100},
    {"Gems", 50}
};
post(stats.Coins);
```

Emits:

```luau
local stats: { [string]: number } = { Coins = 100, Gems = 50 }
print(stats.Coins)
```

## See also

[array](array) · [operator-table](operator-table)
