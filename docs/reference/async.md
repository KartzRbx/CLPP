---
title: "async"
sidebar_label: "async"
---

# async

<div class="clpp-ref-meta">Concurrency</div>

Marks a function that may `await`. Does not emit a Promise wrapper by itself.

## Syntax

```clpp
async T Name(Args args) {
    T x = await expr;
    return x;
}
```

## Parameters

None.

## Return value

`T`.

## Luau emit

`const function  (body uses __await)`

## Description

[`await expr`](await) calls `__await`: if the value has `:expect()` (Promise), wait; otherwise return it (already yielded).

## Example

```clpp
async Data* FetchData(Player* player) {
    Data* data = await DataService:Server::WaitFor(player);
    return data;
}
```

Emits:

```luau
const function FetchData(player: Player): Data
	local data: Data = __await(DataService.Server:WaitFor(player))
	return data
end
```

## See also

[await](await) · [spawn](spawn) · [function](function)
