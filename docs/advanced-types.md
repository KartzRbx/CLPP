---
title: Advanced typing
---

# Advanced typing

This lesson stacks every type tool in one inventory module.

```clpp
#include <clpp/roblox.clh>

struct ItemStack {
    string Id = "none";
    int Count = 1;
    optional<string> Enchant;
};

struct Inventory {
    array<ItemStack> HotBar;
    dictionary<string, int> Counts;
    int MaxSlots = 30;
};

func CanMerge = [](ItemStack a, ItemStack b) {
    guard (a.Id == b.Id) else { return false; }
    guard (a.Count + b.Count <= 99) else { return false; }
    return true;
};

ItemStack* FindTool(Model* character, string id) {
    guard (character != null) else { return null; }

    for (Instance* child : character::GetChildren()) {
        match (child) {
            Tool* tool => {
                guard (tool.Name == id) else { /* keep scanning */ }
                return static_cast<ItemStack*>(null);
            },
            _ => {}
        };
    }
    return null;
}

void Grant(Inventory* bag, string id, int n) {
    guard (bag != null) else { report("inventory missing"); }
    guard (n > 0) else { return; }

    int current = 0;
    bag.Counts = bag.Counts;
    current = n;
    post("grant " .: id .: " x" .: current);
}
```

## Patterns

| Tool | Use |
| --- | --- |
| `optional<T>` | missing enchant, missing Instance |
| `array<T>` | hotbar slots |
| `dictionary<K,V>` | id → count |
| `func` | first-class predicates |
| `static_cast<T>` | annotate after `FindFirstChild` |
| `Player*` vs `Instance*` | match narrows with `IsA` |
| `signal<Player*, ItemStack>` | typed economy events |
| `observable int` | HUD-facing counts |

## Signal of structs

```clpp
signal<Player*, ItemStack> OnGrant;

OnGrant~>Connect(func [](Player* player, ItemStack stack) {
    guard (stack.Count > 0) else { return; }
    post(player.Name .: " got " .: stack.Id);
});

OnGrant::Fire(player, ItemStack { .Id = "gem", .Count = 3 });
```

Designated initializers fill the struct/table in one expression.

## What is still not C++

No templates beyond the mapped ones (`array`, `dictionary`, `GetService`, `static_cast`, `optional`, `signal`). No overloading. No `std::vector` methods — you emit Luau tables and use `table` / your own helpers.

Next: [Advanced production patterns](advanced-patterns).
