# Cluaupp host consumption

CL++ emits a JSON [`CompileArtifact`](../support/cluaupp.d.ts). Cluaupp (separate tool) must:

1. **nativeHints** → selective `@native` via [`applyNativeHints`](../support/cluaupp-bridge.ts) (never blanket).
2. **layoutHints** → SoA / buffer decisions via `planLayout`.
3. **profile** → `cluaupp profile` samples → `build --profile` / `hotFunctions`.
4. **Product** → watch, Rojo sync, typed remotes, DataModel queries, doctor, source maps (`CluauppProduct`).

Compiler hooks live in [`src/platform`](../src/platform/mod.rs) (`capabilities_from_hints`).
