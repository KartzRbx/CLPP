---
title: Advanced typing
---

# Advanced typing

Inventory-style example using aliases, optionals, collections, and `guard` / `match`.

```clpp
type ItemId = string;

struct ItemStack {
    ItemId Id = "none";
    int Count = 1;
    optional<string> Enchant;
};

struct Inventory {
    array<ItemStack> HotBar;
    dictionary<string, int> Counts;
    int MaxSlots = 30;
};

bool CanMerge(ItemStack a, ItemStack b) {
    guard (a.Id == b.Id) else { return false; }
    guard (a.Count + b.Count <= 99) else { return false; }
    return true;
}

void Grant(Inventory bag, ItemId id, int n) {
    guard (n > 0) else { return; }
    post("grant " .: id .: " x" .: n);
}
```

## Patterns

| Tool | Use |
| --- | --- |
| `optional<T>` | missing values; narrow with `guard` |
| `type` / `using` | name a shape without a new nominal |
| `A \| B` | open union |
| `template <typename T : B>` | checked generics |
| `link` | pull a module, `@clpp`, or `@game` |

See [Types](types) · [TYPE_SYSTEM](architecture/TYPE_SYSTEM) · [Modules](modules).
