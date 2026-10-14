# Mental model

This chapter distinguishes intended language semantics from the current implementation. **The official execution model is CL++ bytecode on the CL++ VM.** Target examples teach design requirements that still need compiler alignment.

## The central idea

> **Types help the compiler understand your source; the compiler generates bytecode, and the CL++ VM executes it.**

A developer can write a transpiler in CL++ as an ordinary application. That application could generate Luau or another language, but its output format, type erasure, and host integration would belong to the application. They are not an official alternative CL++ backend.

Neither static types nor generation of another language establish zero overhead. Object allocation, conversions, protected calls, and connection cleanup can cost time. Measure the actual program and runtime rather than assuming every design abstraction is free.

## The path from source to execution

```text
source .clp → tokens → syntax tree → names and scopes → type checks → bytecode → CL++ VM
```

| Stage | Responsibility | Typical failure |
| --- | --- | --- |
| Directives | Interpret requested checking modes, such as target `pragma` forms | Unknown or unsupported directive |
| Lexer | Separate source characters into tokens | Invalid character or unfinished literal |
| Parser | Build a syntax tree | Missing punctuation or malformed declaration |
| Name resolution | Determine which declaration each name refers to | Unresolved name |
| Type checker | Check compatible values and intended optional/match rules | Type or contract diagnostic |
| Code generation | Emit bytecode after successful checks | Invalid or unsupported lowering |
| VM | Execute verified bytecode | Runtime failure |

Not every target check exists yet. A syntax error may prevent a usable tree from being formed; a type error concerns code that could be parsed. Fix the first relevant diagnostic rather than treating every later message as an independent problem.

## Types during compilation and execution

| Concept | Compile-time role | Execution contract |
| --- | --- | --- |
| `number`, `float`, `bool`, `string` | Describe distinct accepted values | The VM operates on runtime values; types are not a claim of Luau emission |
| `number` versus `float` | Distinguish counts from fractional measurements | Current integer division discards the fractional part |
| `def` | Describe a named object type or alias | Target object identity still requires implementation |
| `optional<T>` | Require absence handling before contained-value use | Target representation and narrowing need alignment |
| Context annotations | Express proposed checking constraints | Host-specific authority rules belong to the host project |
| `const` | Prohibit reassignment of a binding | Typed target spelling needs parser migration |

A type annotation is not validation of data arriving from an external API. Conversion, rejection, and failure behavior require explicit contracts and tests.

## An educational transpiler example

If you write a CL++ application that emits Luau, you might map `number` and `float` to `number`, `bool` to `boolean`, `string` to `string`, object definitions to tables, and absent values to `nil`. Compile-time annotations could disappear or become output annotations. These are **choices your transpiler must implement**, not CL++'s official runtime model.

Preserve source behavior deliberately. In Luau, `5 / 2` produces `2.5`; current CL++ integer division drops the fractional part. Simply replacing tokens would not preserve that behavior. A transpiler must account for conversions, optional checks, context rules, and any cleanup helpers it introduces.

## Objects have identity and shared references

The author's target design expects an object defined with a `def` body to have identity and shared references. Do not infer every vector or buffer's semantics merely because it is not a number. The VM implements shared identity for `def`, arrays and dictionaries; primitive values keep value semantics.

```clp
def Point { number X=0; number Y=0; }
Point first=Point { X=1 }; Point second=first; second.X=99;
post(first.X); post(first==second); Point independent=Point { X=99 }; post(first==independent);
```

```output
99
true
false
```

This differs from an ordinary C++ value copy. Create a separate `Point` explicitly when you need independent state.

Under the target object model, `first == second` asks whether the two values refer to **the same object**, not whether every field matches. Write a function such as `SamePosition(first, second)` for field-by-field comparison.

## No manual pointer management

C++ uses `*`, `&`, and `->` in pointer operations, and supports pointer arithmetic and `delete`. CL++'s design does not include that manual memory model; the same characters can have other roles in expressions or callback types.

- Runtime memory management belongs to the CL++ implementation rather than source-level pointer deletion.
- Write `Player player`, not a C++ pointer declaration.
- Use `.` for instance members and `::` for scoped/static access.
- `->` appears in callback signatures, such as `def Logger = (string) -> void;`; it is not pointer-member access.

## `null` means absence

`optional<T>` permits absence. The shorter `T?` form is proposed by the supplied handbook and still needs validation. The intended checker should reject absent values in an ordinary `number` and require narrowing before an optional is used as `T`.

Prefer an explicit presence check: `value != null`. The current VM treats zero and empty text as false in conditions. A custom transpiler to Luau would also need to handle Luau's different truthiness: zero and empty text are true there. Neither runtime's ordinary truthiness automatically implements a target optional-presence rule.

## What CL++ is not

CL++ borrows familiar C++ writing, but it is its own language. The target design does not include C++ function overloading, default function arguments, C++ reference declarations, explicit capture lists, macros, header inclusion, or full C++ templates. Generics and callbacks follow their own contracts; see [Reference](./14-referencia.md).

These limits keep the design focused rather than making it a complete C++ implementation.

## Four useful habits

1. **Declare intent.** Use `auto` when the initializer makes the type clear.
2. **Handle absence.** An optional value requires a deliberate missing-value path.
3. **Read diagnostics.** A precise compile-time failure is easier to investigate than a late bug.
4. **Check implementation status.** Distinguish an intended rule from behavior of the installed executable. If your own application generates source, inspect that output as part of testing it.
