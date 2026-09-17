---
title: "struct / class"
sidebar_label: "struct"
---

# struct / class

<div class="clpp-ref-meta">OOP</div>

Declare the type in a `.clh`. Implement `Class::Method` in the sibling `.clp` / `.clpp`.

## Syntax

```clpp
struct LeaderstatsServer {
    Janitor* janitor;
    void OnPlayer(Player* player);
};
```

## Parameters

None.

## Return value

A type (and, for field-only headers, a constructor function).

## Luau emit

`export type + function Class:Method / const function Name() for field-only`

## Description

`class` is a synonym of `struct`. [`public:` / `private:` / `protected:`](access-labels) are ignored.

Field-only structs in a header emit `const function Name()` with defaults (DataStore templates). Nested structs with defaults become nested tables.

Stems must match: `LeaderstatsServer.clh` beside `LeaderstatsServer.server.clpp`.

Instances are not RAII. Leaving a block does **not** `Destroy` — use Janitor.

## Example

```clpp
struct LeaderstatsServer {
    Janitor* janitor;
};
```

Emits:

```luau
-- export type LeaderstatsServer = { janitor: Janitor, ... }
```

## See also

[class-method](class-method) · [this](this) · [init](init) · [access-labels](access-labels)
