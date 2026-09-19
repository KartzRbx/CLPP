---
title: Declarative UI with Fusion
---

# Declarative UI with Fusion

[Fusion](https://elttob.uk/Fusion) builds UI from **state** and **trees of instances**. CL++ talks to it like any other table library: `Fusion.scoped`, `Fusion.New`, `Fusion.Value`.

Cluaupp (or your Rojo tree) should `require` Fusion. In CL++ you treat it as a global table, same as `DataService`.

## Mental model

1. Create a **scope** (owns all state and instances; destroy the scope to clean up).
2. Put numbers/strings you care about in `Value`.
3. `New(scope, "ClassName")` returns a constructor. Call it with a `dictionary` of properties.
4. Nested instances go under the Fusion `Children` key — a dictionary pair `{Children, child}`.

## Coins HUD

```clpp
#include <clpp/roblox.clh>

void init() {
    Players players = GetService<Players>();
    Player localPlayer = players.LocalPlayer;
    guard (localPlayer != null) else {
        report("LocalPlayer missing");
        return;
    }

    PlayerGui playerGui = localPlayer.WaitForChild("PlayerGui");
    auto scope = Fusion.scoped();
    auto Children = Fusion.Children;

    auto coins = Fusion.Value(scope, 0);
    auto title = Fusion.Computed(scope, func () {
        return "Coins: " .: coins();
    });

    auto newScreenGui = Fusion.New(scope, "ScreenGui");
    auto newFrame = Fusion.New(scope, "Frame");
    auto newLabel = Fusion.New(scope, "TextLabel");

    auto label = newLabel({
        {"Name", "Amount"},
        {"BackgroundTransparency", 1},
        {"Size", UDim2.fromScale(1, 1)},
        {"Text", title},
        {"TextColor3", Color3.fromRGB(255, 255, 255)},
        {"Font", Enum.Font.GothamBold},
        {"TextScaled", true}
    });

    auto frame = newFrame({
        {"Name", "Hud"},
        {"AnchorPoint", Vector2(0, 1)},
        {"Position", UDim2.fromScale(0, 1)},
        {"Size", UDim2.fromOffset(220, 64)},
        {"BackgroundColor3", Color3.fromRGB(28, 18, 48)},
        {Children, label}
    });

    newScreenGui({
        {"Name", "CoinsHud"},
        {"ResetOnSpawn", false},
        {"Parent", playerGui},
        {Children, frame}
    });

    // Game code fires the HUD by writing Fusion state:
    coins(100);
}
```

## Events and cleanup

Fusion's `OnEvent` is a table key, just like `Children`:

```clpp
auto OnEvent = Fusion.OnEvent;
auto newButton = Fusion.New(scope, "TextButton");

newButton({
    {"Text", "+10"},
    {OnEvent("Activated"), func () {
        coins(coins() + 10);
    }}
});
```

When the scope is destroyed, Fusion destroys instances and disconnects. You still use CL++ `~>` for **engine** signals outside Fusion (`PlayerAdded`, DataService).

## Pattern with guard + match

Drive the same HUD from inventory instances:

```clpp
void BindDrop(Instance inst, auto coins) {
    match (inst) {
        IntValue v => {
            guard (v.Name == "Coins") else { return; }
            coins(v.Value);
            v.OnChange(func (int n) { coins(n); });
        },
        _ => warn("not a coins value")
    };
}
```

Full file: [`examples/ui/FusionHud.client.clpp`](https://github.com/KartzRbx/CLPP/blob/main/examples/ui/FusionHud.client.clpp).

Next: [Vide](ui-vide).
