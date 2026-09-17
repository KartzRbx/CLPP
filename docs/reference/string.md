---
title: "string"
sidebar_label: "string"
---

# string

<div class="clpp-ref-meta">Type</div>

UTF-8 text. Not `std::string`. Concatenate with [`.:`](operator-concat).

## Syntax

```clpp
string name = value;
```

## Parameters

None.

## Return value

A value of type `string`, emitted as `string`.

## Luau emit

`string`

## Description

[`observable string`](observable) becomes `StringValue`. Double quotes only in the current grammar. There is no string_view.

## Example

```clpp
string name = "Kartz";
string key = name .: "_LeaderstatsJanitor";
```

Emits:

```luau
local name: string = "Kartz"
local key: string = name .. "_LeaderstatsJanitor"
```

## See also

[operator-concat](operator-concat) · [string_concat](string_concat)
