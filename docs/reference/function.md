---
title: "function"
sidebar_label: "function"
---

# function

<div class="clpp-ref-meta">Functions</div>

Named function. Only bodies in `.clp` / `.clpp` emit. Prototypes in `.clh` become `export type` fields.

## Syntax

```clpp
ReturnType Name(T arg) {
    return arg;
}
```

## Parameters

None.

## Return value

Whatever `ReturnType` is.

## Luau emit

`const function Name(arg: T): ReturnType`

## Description

No overloading. No default arguments. No templates except [`GetService<T>`](GetService) / collections / [`static_cast`](static_cast).

[`async`](async) functions may [`await`](await). [`void init()`](init) is the script entry.

## Example

```clpp
void CreateLeaderstats(Player player) {
    return;
}

const int DoubleCoins(int coins) {
    return coins;
}
```

Emits:

```luau
const function CreateLeaderstats(player: Player)
	return
end

const function DoubleCoins(coins: number): number
	return coins
end
```

## See also

[lambda](lambda) · [async](async) · [init](init) · [class-method](class-method)
