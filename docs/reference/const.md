---
title: "const / constexpr"
sidebar_label: "const"
---

# const / constexpr

<div class="clpp-ref-meta">Type qualifier</div>

`const` and `static constexpr` become Luau `const`.

## Syntax

```clpp
const int DoubleCoins(int coins) {
    return coins;
}
static constexpr int CAP = 100;
```

## Parameters

None.

## Return value

A const binding.

## Luau emit

`const / const function`

## Description

Function declarations emit `const function`. File-level constants emit `const`. There is no `mutable`. `public:` / `private:` do not affect constness.

## Example

```clpp
const int DoubleCoins(int coins) {
    return coins;
}
```

Emits:

```luau
const function DoubleCoins(coins: number): number
	return coins
end
```

## See also

[function](function) · [struct](struct)
