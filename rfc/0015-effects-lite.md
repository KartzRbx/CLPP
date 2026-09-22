# RFC 0015 — Effects lite

Status: Draft — surface reserved

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface (planned)

```clpp
void save(Player p) effect(io, yields) { … }
```

Tracked effect sets: `pure`, `io`, `yields`, `mutates`. Checker warns when a `pure` function calls `io`.

## Non-goals

Full algebraic effect handlers / resumable effects (Phase C research).
