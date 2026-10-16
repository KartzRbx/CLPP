---
title: Collections
description: Ordered arrays, keyed dictionaries, mathematical vectors, iteration, and collection identity.
---

# Collections

> **Current collection contract.** The complete examples below execute in the documentation suite. Optional missing-key lookup remains an explicitly deferred target block; the current dictionary reports an absent key as an error. Luau/Roblox examples describe an application written in CL++, not an official backend. See [compiler status](../reference/status.md).

A collection holds several values under one name. The supplied design discusses `array`, `dictionary`, and mathematical vectors. The current VM supplies arrays, dictionaries, and `Vector2`, `Vector3`, and `Vector4`. The source's Luau `vector` comparison is not a new official backend or a verified CL++ type spelling.

## Ordered arrays

```clp
array<number> grades = array<number>(10, 8, 9);
array<string> names = array<string>();
push(grades, 7);
number first = grades[0];
post(first);
post(len(grades));
```

```output
10
4
```

Use an array when order matters or values are accessed by position. `array<number>` constrains elements to integers. `push` adds an element at the end. The VM has global collection operations; do not assume source illustrations such as `.size()` or `.push()` are supported methods.

### Iteration

```clp
array<number> grades = array<number>(10, 8, 9, 7);
for (number grade in grades) { post(grade); }
```

```output
10
8
9
7
```

Typed iteration checks the element type. See [Control flow](./05-controle-de-fluxo.md) for indexed loops.

### Index bases

The current CL++ VM uses **zero-based** indices: `values[0]` is the first element. Luau's usual one-based array convention is relevant only when implementing a transpiler application in CL++. That application must choose and test an index translation; it is not automatic official compiler behavior.

## Vectors for mathematics

Use small fixed-size vectors for positions and directions. The VM has `Vector2`, `Vector3`, and `Vector4`; arrays hold arbitrary sequences of items. The supplied manual's lowercase `vector` refers to a Luau comparison and requires a separately specified adapter or application type before use.

## Keyed dictionaries

```clp
dictionary<string, number> scores = dictionary<string, number>();
scores["ada"] = 120;
scores["rui"] = 80;
number score = scores["ada"];
post(score);
post(scores["rui"]);
```

```output
120
80
```

A dictionary associates keys with values. `dictionary<string, number>` maps strings to integers. Use it for “what is Ada's score?” rather than “what is the third score?”

### Missing keys

The supplied design illustrates optional lookup as a future proposal. The author
chose to preserve the current checked-error lookup behavior:

```clp target
optional<number> missing = scores["unknown"];
```

That optional-return contract is unresolved. In the current VM, reading an absent dictionary key raises a runtime failure; assignment to `optional` does not automatically catch it. Use `try/catch` for that current behavior. A transpiler targeting another language must explicitly preserve or redefine the lookup contract rather than inherit its host's behavior accidentally.

## Signals and listeners

A `signal` represents listeners rather than ordinary stored data. Generic signal signatures and callback contracts are discussed in [Events and concurrency](./11-eventos-e-concorrencia.md); a source type label does not prove its implementation.

## Choosing a container

| Need | Use |
| --- | --- |
| Ordered values | `array<T>` |
| Lookup by name or identifier | `dictionary<K, V>` |
| Coordinates and mathematical operations | The appropriate vector type |
| Each element of an iterable collection | `for (T value in collection)` |

## Identity and mutation

Collections retain identity when passed to a function:

```clp
void AddItem(array<number> values) {
    push(values, 1);
}
auto values = array<number>();
AddItem(values);
post(values);
```
```output
[1]
```

Assignment and parameter passing preserve the same collection. Construct another collection for an independent container; object elements can still be shared. `const` freezes reachable contents through every alias. Copy APIs and transfer semantics require their own contracts.
