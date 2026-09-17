---
title: "public / private / protected"
sidebar_label: "public:"
---

# public / private / protected

<div class="clpp-ref-meta">OOP</div>

Parsed and ignored. Luau has no access specifiers.

## Syntax

```clpp
struct S {
public:
    int x;
private:
    int y;
};
```

## Parameters

None.

## Return value

None.

## Luau emit

`(omitted)`

## Description

They exist so C++-looking headers parse. Encapsulation is by ModuleScript surface, not the compiler.

## Example

```clpp
struct S {
public:
    int x;
};
```

Emits:

```luau
-- fields still emit on the type
```

## See also

[struct](struct)
