---
title: "++ / −−"
sidebar_label: "++ −−"
---

# ++ / −−

<div class="clpp-ref-meta">Operator</div>

Increment / decrement. Always emits `+= 1` / `-= 1` (not a distinct prefix/postfix value).

## Syntax

```clpp
i++;
++i;
i--;
```

## Parameters

None.

## Return value

Used as a statement; the C++ value distinction is not preserved.

## Luau emit

`i += 1  /  i -= 1`

## Description

Typical in [C-style for](for). Do not rely on `x = i++` returning the old value.

## Example

```clpp
for (int i = 0; i < 10; i++) {
    post(i);
}
```

Emits:

```luau
local i = 0
while i < 10 do
	print(i)
	i += 1
end
```

## See also

[for](for) · [operator-assignment](operator-assignment)
