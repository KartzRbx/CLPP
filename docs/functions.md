---
title: Functions and callbacks
---

# Functions and callbacks

```clpp
void CreateLeaderstats(Player player) {
    return;
}

const int DoubleCoins(int coins) {
    return coins;
}
```

- Only functions **with a body** emit from `.clp` / `.clpp`.
- Prototypes in `.clh` become `export type` fields.
- No overloading. No default arguments.
- `void init()` is the script entry. No `int main()`.

## Anonymous callbacks

Write `func (params) { }`. There are no C++ captures `[x]` / `[&]`.

```clpp
func onCoinsChanged = func (int newValue) {
    post("New value: " .: newValue);
};

players.PlayerAdded~>Connect(func (Player playerEntered) {
    post("New player: " .: playerEntered.Name);
});
```

Luau still closes over outer locals. Keep Instances alive with Janitor.

Passing a method by name from inside `Class::` binds `self`.

## `async`

```clpp
async Data FetchData(Player player) {
    Data data = await DataService.Server.WaitFor(player);
    return data;
}
```

Covered in [Concurrency](concurrency). Next: [Control flow](control-flow).
