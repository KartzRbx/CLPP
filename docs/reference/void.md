---
title: "void"
sidebar_label: "void"
---

# void

<div class="clpp-ref-meta">Type</div>

No value. Return type of procedures. `void init()` is the script entry.

## Syntax

```clpp
void Name(T arg) {
    return;
}
```

## Parameters

None.

## Return value

Nothing. Luau omits the return annotation (or uses `()` internally).

## Luau emit

`(no return annotation)`

## Description

`return;` is valid. `return expr;` in a `void` function should be avoided. `void` is only a return type.

## Example

```clpp
void CreateLeaderstats(Player player) {
    return;
}
```

Emits:

```luau
const function CreateLeaderstats(player: Player)
	return
end
```

## See also

[init](init) · [function](function)
