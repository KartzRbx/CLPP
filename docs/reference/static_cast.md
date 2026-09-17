---
title: "static_cast"
sidebar_label: "static_cast"
---

# static_cast

<div class="clpp-ref-meta">Builtin · cast</div>

Source-level type annotation. Luau has no runtime casts — this does not check `ClassName`.

## Syntax

```clpp
T static_cast<T>(value);
```

## Parameters

| Name | Type | Description |
| --- | --- | --- |
| `T` | `type` | Target type, often `Folder*` / `Player*`. |
| `value` | `any` | Expression to re-annotate. |

## Return value

`value` unchanged, annotated as `T`.

## Luau emit

`value  (with a Luau type annotation)`

## Description

`const_cast`, `reinterpret_cast`, and `dynamic_cast` also emit the argument. `(void)x;` is dropped (silences unused in clangd).

To branch on Instance class at runtime, use [`match`](match) (`IsA`).

## Example

```clpp
Folder* folder = static_cast<Folder*>(existingFolder);
```

Emits:

```luau
local folder: Folder = existingFolder
```

## See also

[match](match) · [instance-pointer](instance-pointer) · [auto](auto)
