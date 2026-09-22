# Studio benches A–F (fair baselines)

Fixtures live in [`benches/`](../../benches/). Host compile timing is recorded in [`results.json`](results.json).

## Suites

| Suite | Focus | Fixture |
| --- | --- | --- |
| A | Numeric hot loops | `benches/numeric.clpp` |
| B | Dense data / SoA candidates | `benches/data.clpp` |
| C | Realistic gameplay mix | `benches/realistic.clpp` |
| D | Memory / GC pressure | data + array growth |
| E | Opt compare (`--no-opt` vs opt) | any |
| F | `@native` via `nativeHints` | hot functions from A |

## Baselines (must all be measured in Studio)

1. Idiomatic Luau
2. Hand-tuned Luau
3. roblox-ts emit
4. CL++ `--no-opt`
5. CL++ opt
6. CL++ opt + selective `@native` (Cluaupp)

Report **P50 / P95 / P99**, GC time, and memory footprint. Do not invent numbers — leave null in `results.json` until Studio runs complete.
