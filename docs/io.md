---
title: I/O — post, warn, report
---

# I/O — post, warn, report

| CL++ | Luau | Use |
| --- | --- | --- |
| `post(...)` | `print(...)` | normal output |
| `warn(...)` | `warn(...)` | recoverable problems |
| `report(...)` | `error(...)` | stop the thread |

```clpp
post("ok");
warn("careful");
report("failed");
```

Legacy C++ streams still parse: `cout << "hi" << endl` — each `<<` is another argument, `endl` ends the line. Prefer `post`.

There is no `scanf`. Game data comes from Instances, DataStores (via Cluaupp/DataService), and signals.

See [Builtins](/CLPP/api/Builtins). Next: [Roblox instances](roblox).
