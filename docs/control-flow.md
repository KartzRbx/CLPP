---
title: Control flow
---

# Control flow

## if

```clpp
if (coins > 50) {
    post("Enough balance!");
} else if (coins == 0) {
    warn("No coins!");
} else {
    report("Balance sync error.");
}
```

`else if` emits `elseif`.

## while and for

```clpp
while (true) {
    post("tick");
}

for (int i = 0; i < 10; i++) {
    post("Count: " .: i);
}

for (Player* player in players.GetPlayers()) {
    post("Player connected: " .: player.Name);
}
```

C-for counts. Range-for uses `in` (or `:`) to name the collection.

## switch

Evaluates the discriminant **once**. Stacked `case`s share a body. `break` leaves the switch. No C fall-through.

```clpp
switch (action) {
case "buy":
case "purchase":
    Grant(player);
    break;
default:
    warn("unknown");
    break;
}
```

## Not in the language

`continue`, ternary `? :`, `do/while`, `goto`, `try/catch`.

`guard` and `match` have their own lesson: [Guard and match](guard-match).

Next: [Collections](collections).
