# RFC 0018 — Non-copyable / move

Status: Draft — surface reserved

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface (planned)

```clpp
struct unique Buffer {
  …
};
void consume(Buffer b by move);
```

After move, the source binding is unusable (checker error). No C++ references/pointers.

## Emit

Same table pointer; uniqueness is a compile-time property only.
