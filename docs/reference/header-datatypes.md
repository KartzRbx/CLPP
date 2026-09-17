---
title: "<clpp/datatypes.clh>"
sidebar_label: "datatypes.clh"
---

# `<clpp/datatypes.clh>`

<div class="clpp-ref-meta">Standard header</div>

Vector3, CFrame, UDim2, Color3, and `string_concat`.

## Syntax

```clpp
#include <clpp/datatypes.clh>
```

## Parameters

None.

## Return value

None.

## Luau emit

`datatype constructors stay global`

## Description

Datatypes are **values**: `Vector3(8, 1, 8)`, not `new Vector3`. Methods/static names use [`::`](operator-method) which emits `.` for datatypes.

## Example

```clpp
part.Size = Vector3(8, 1, 8);
part.CFrame = CFrame.lookAt(from, look);
```

Emits:

```luau
part.Size = Vector3.new(8, 1, 8)
part.CFrame = CFrame.lookAt(from, look)
```

## See also

[new](new) · [string_concat](string_concat) · [operator-method](operator-method)
