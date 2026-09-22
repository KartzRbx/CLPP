# RFC 0010 — Checked generics (Intent Phase A #2)

Status: Accepted for implementation (subset)

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Problem

C++-style templates delay type errors until instantiation. Carbon-style **checked generics** verify the generic body against declared **bounds** at definition time, so every valid instantiation is safe.

CL++ already parses `template <typename T>` for functions/structs but does not check bodies against constraints. This RFC adds a minimal bound surface — **not** full C++ templates ([DEFERRED.md](../docs/architecture/DEFERRED.md)).

## Syntax (subset)

```text
interface Drawable {
    void render();
};

template <typename T : Drawable>
void draw(T item) {
    item.render();   // OK — bound guarantees render
}

struct Circle { void render(); int r; };
struct Plain { int n; };

void F(Circle c, Plain p) {
    draw(c);   // OK — Circle satisfies Drawable
    draw(p);   // error CLPP0901
}
```

- Bound form: `typename T : BoundName` (nominal interface/struct).
- Unbounded `typename T` remains legal; **member access on unbounded `T` is an error**.

## Typing rules

1. **Definition:** type parameter `T : B` is a `GenericParam` whose members are those of `B`.
2. **Body:** member access on `T` is checked against `B` only (CLPP0604 / CLPP0901).
3. **Call:** each concrete type argument (explicit or inferred from a parameter of type `T`) must **satisfy** `B`:
   - nominal subtype of `B`, or
   - structural: every member of `B` exists on the concrete type.
4. **Assignability:** value `V` assigns to parameter typed `T : B` iff `V` satisfies `B`. `T` assigns to `U` if `B` is a subtype of `U`.

## Emit

Luau type parameters keep names only (`function draw<T>(item)`); bounds are compile-time only.

## Non-goals

- Variadic / pack templates, SFINAE, specialization
- Multi-bound intersections (`T : A + B`) — later
- Associated types / where-clauses
- Checking generic **struct** field layouts beyond param name plumbing

## Implementation map

| Layer | Location |
| --- | --- |
| Grammar | `template_param` optional `: ident` |
| AST | `TypeParam { name, bound }` |
| Types | `TypeKind::GenericParam { name, bound }`, `satisfies_bound`, `fresh_generic` |
| Checker | `typed` member + call-site bounds (CLPP0901) |
| Tests | `tests/v07.rs` |

## Success

- Body using unbound `T.member` fails
- Body using `T : Bound` member OK when member ∈ Bound
- Call with type that lacks bound members → CLPP0901
- `cargo test` green
