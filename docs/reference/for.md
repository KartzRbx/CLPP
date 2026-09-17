---
title: "for (C-style)"
sidebar_label: "for"
---

# for (C-style)

<div class="clpp-ref-meta">Control flow</div>

C `for`. Emits `while` plus a trailing increment. Semicolons separate clauses.

## Syntax

```clpp
for (int i = 0; i < n; i++) {
}
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `init` | `stmt` | Runs once. |
| `cond` | `bool` | Checked every iteration. |
| `step` | `expr` | Usually `i++`. |

## Return value

None.

## Luau emit

`local i = 0 / while i < n do / i += 1`

## Description

The `:` of [range-for](range-for) is a different form: `for (T name : collection)`. Do not mix them.

## Example

```clpp
for (int i = 0; i < 10; i++) {
    post("Count: " .: i);
}
```

Emits:

```luau
local i = 0
while i < 10 do
	print("Count: " .. i)
	i += 1
end
```

## See also

[range-for](range-for) · [while](while) · [operator-increment](operator-increment)
