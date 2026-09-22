# RFC 0016 — Typestate

Status: Draft — surface reserved

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface (planned)

```clpp
struct File: Closed {
  File open(string path) -> Open;
  void close(File: Open) -> Closed;
}
```

Methods consume/produce phantom state tags; illegal transitions are type errors.

## Emit

States erased; only the checker enforces transitions.
