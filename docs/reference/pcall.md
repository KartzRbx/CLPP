---
title: "pcall"
sidebar_label: "pcall"
---

# pcall

<div class="clpp-ref-meta">Builtin · error handling</div>

Protected call. CL++ has no `try/catch`; this is how you catch [`report`](report) / Luau `error`.

## Syntax

```clpp
auto [ok, result] = pcall(fn);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `fn` | `func` | Callback to run. Usually a lambda. |

## Return value

Multiple values: success flag, then the result or the error message.

## Luau emit

`pcall(fn)`

## Description

Pair with [destructuring](destructure). If `ok` is false, `result` is the error string. There is also Luau `xpcall` if you include it as a global — the compiler treats it as a call.

## Example

```clpp
auto [success, result] = pcall(func []() {
    return DataStore::GetAsync("PlayerData");
});
```

Emits:

```luau
local success, result = pcall(function()
	return DataStore:GetAsync("PlayerData")
end)
```

## See also

[report](report) · [destructure](destructure) · [lambda](lambda)
