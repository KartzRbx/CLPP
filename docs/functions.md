---
title: Functions and lambdas
---

# Functions and lambdas

```clpp
void CreateLeaderstats(Player* player) {
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

## Lambdas

Empty `[]` — no C++ captures `[x]` / `[&]`. Prefix with `func` when you want the type:

```clpp
func onCoinsChanged = [](int newValue) {
    post("New value: " .: newValue);
};

players::PlayerAdded::Connect(func [](Player* playerEntered) {
    post("New player: " .: playerEntered.Name);
});
```

Luau still closes over outer locals. Keep Instances alive with Janitor.

Passing a method by name from inside `Class::` binds `self`.

## `async`

```clpp
async Data* FetchData(Player* player) {
    Data* data = await DataService:Server::WaitFor(player);
    return data;
}
```

Covered in [Concurrency](concurrency). Next: [Control flow](control-flow).
