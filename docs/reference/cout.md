---
title: "cout / endl"
sidebar_label: "cout"
---

# cout / endl

<div class="clpp-ref-meta">I/O · legacy stream</div>

C++ iostream leftover. Parses, but `post` is the language's real print.

## Syntax

```clpp
cout << arg << arg << endl;
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `arg` | `any` | Each `<<` is another print argument. |
| `endl` | `manipulator` | Ends the line. |

## Return value

`void`.

## Luau emit

`print(arg, arg)`

## Description

Each `<<` becomes another argument to `print`. `endl` finishes the statement. There is no `cin`, `scanf`, or stream formatting (`std::setw`). Game input comes from Instances, DataStores, and signals.

## Notes

New code should use [`post`](post).

## Example

```clpp
cout << "hi" << endl;
```

Emits:

```luau
print("hi")
```

## See also

[post](post)
