---
title: Cluaupp
---

# Cluaupp

[Cluaupp](https://github.com/KartzRbx/Cluaupp) is the **Roblox connection**: generated instance headers, Rojo, project `init`, package install.

CL++ is the **language**. This split matters:

| Question | Owner |
| --- | --- |
| Does `::` vs `.` compile? | CL++ |
| Why is `ProximityPrompt` missing from IntelliSense? | Cluaupp headers |
| How does a `.server.clpp` become a Script in Studio? | Cluaupp + Rojo, using CL++ tags |
| JSON compile for a bundler | `clpp api compile` (stable contract) |

```bash
clpp api compile --file src/server/Leaderstats.server.clpp
clpp api manifest
```

**Updating the Cluaupp CLI to this compiler:** [Update Cluaupp for CL++ 0.3.2](cluaupp-032) — pin **0.3.3**.

TypeScript types: `support/cluaupp.d.ts`. Details: [Cluaupp support](cluaupp-support).

Anonymous callbacks Cluaupp must generate: [Cluaupp — anonymous callbacks](cluaupp-callbacks). Write `func (params) { }`. `func [](…)` and `[]() { }` do not compile.
