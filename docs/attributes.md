---
title: Server and client attributes
---

# Server and client attributes

```clpp
[[server]]
void SaveData(Player player) {
    post("saving " .: player.Name);
}

[[client]]
void UpdateUI() {
    post("ui");
}
```

| File | `[[server]]` | `[[client]]` |
| --- | --- | --- |
| `*.server.clpp` | kept | omitted |
| `*.client.clpp` | omitted | kept |
| Module (no tag) | wrapped in `RunService:IsServer()` | `IsClient()` |

Use attributes instead of copying whole files. Shared modules can expose both functions; the emit guards them.

Next: [Declarative UI with Fusion](ui-fusion).
