---
title: Observable and signals
---

# Observable and signals

Two ways to broadcast change: **observable** (a ValueBase) and **signal** (a typed BindableEvent).

## observable — fire by assigning

```clpp
observable int coins = 100;

coins.OnChange(func [](int newValue) {
    post("Coins changed to: " .: newValue);
});

coins = 50;   // writes .Value and fires Changed
post(coins);  // reads .Value
```

| `T` | Instance |
| --- | --- |
| `int` | `IntValue` |
| `float` `double` | `NumberValue` |
| `string` | `StringValue` |
| `bool` | `BoolValue` |
| anything else | `ObjectValue` |

## signal — fire with `::Fire`

```clpp
signal<Player*, int> OnCoinsUpdated;

OnCoinsUpdated~>Connect(func [](Player* player, int amount) {
    post(player.Name .: ": " .: amount);
});

OnCoinsUpdated~>Once(func [](Player* player, int amount) {
    post("first pay");
});

OnCoinsUpdated::Fire(player, 500);
OnCoinsUpdated::Wait();
```

| API | Role |
| --- | --- |
| `::Fire(...)` | send |
| `~>Connect` | listen until disconnected |
| `~>Once` | listen **once** |
| `::Wait()` | yield until next fire |

## Property change “shots”

```clpp
humanoid::GetPropertyChangedSignal("Health")~>Connect(func []() {
    post(humanoid.Health);
});
```

API pages: [Signals](/CLPP/api/Signals), [Observables](/CLPP/api/Observables).

Next: [Guard and match](guard-match).
