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
| `fn` | `func` | Callback to run. Usually `func (…) { }`. |

## Return value

Multiple values: success flag, then the result or the error message.

## Luau emit

`pcall(fn)`

## Description

Pair with [destructuring](destructure). If `ok` is false, `result` is the error string. There is also Luau `xpcall` if you include it as a global — the compiler treats it as a call.

For a **single** method that must not stop the script, [`:`](operator-table) is the short form: `workspace:FindFirstChild("x")` emits `pcall` and yields `nil` on error.

## Example

```clpp
auto [success, result] = pcall(func () {
    return DataStore.GetAsync("PlayerData");
});
```

Emits:

```luau
local success, result = pcall(function()
	return DataStore:GetAsync("PlayerData")
end)
```

## See also

[report](report) · [operator-table](operator-table) · [destructure](destructure) · [lambda](lambda)
