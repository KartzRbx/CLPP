---
title: Declarative UI with Vide
---

# Declarative UI with Vide

[Vide](https://centau.github.io/vide/) is a fine-grained reactive UI library. Sources are **callable**: `count()` reads, `count(n)` writes. In CL++ that is a normal call.

## Counter

```clpp
#include <clpp/roblox.clh>

void init() {
    Players players = GetService<Players>();
    Player localPlayer = players.LocalPlayer;
    guard (localPlayer != null) else { return; }
    PlayerGui playerGui = localPlayer.WaitForChild("PlayerGui");

    auto create = Vide.create;
    auto source = Vide.source;
    auto apply = Vide.apply;

    auto count = source(0);

    auto makeGui = create("ScreenGui");
    auto makeButton = create("TextButton");
    auto makeLabel = create("TextLabel");

    auto label = makeLabel({
        {"BackgroundTransparency", 1},
        {"Size", UDim2.fromScale(1, 0.5)},
        {"TextColor3", Color3.fromRGB(240, 240, 255)},
        {"Text", func () {
            return "Clicks: " .: count();
        }}
    });

    auto button = makeButton({
        {"Size", UDim2.fromScale(1, 0.5)},
        {"Position", UDim2.fromScale(0, 0.5)},
        {"Text", "Once-safe click"},
        {"Activated", func () {
            count(count() + 1);
        }}
    });

    auto gui = makeGui({
        {"Name", "VideCounter"},
        {"ResetOnSpawn", false},
        {"Parent", playerGui}
    });

    auto mount = apply(gui);
    mount({ label, button });
}
```

Vide `Text` can be a **function**. CL++ lambdas `func () { return ...; }` become that function. Vide re-runs it when `count()` changes.

## Vide + CL++ Once

If a Roblox event should update UI **once** (tutorial toast):

```clpp
workspace.ChildAdded~>Once(func (Instance child) {
    match (child) {
        BasePart p => count(count() + 1),
        _ => warn("ignored")
    };
});
```

`~>` still uses Janitor. Vide sources stay independent of that connection.

Full sample lives in the Cluaupp repo (Roblox UI host), not under CL++ `examples/`.

Next: [Advanced typing](advanced-types).
