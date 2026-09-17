---
title: Guard and match
---

# Guard and match

These two constructs replace a large class of nested `if`s.

## guard — fail fast

If the condition is **false**, the `else` block runs. Almost always you `return` (or `report`).

```clpp
void LeaderstatsServer::PlayerEntered(Player* player) {
    guard (player != null) else {
        warn("Invalid player");
        return;
    }

    guard (player.Parent != null) else {
        return;
    }

    post(player.Name);
}
```

```luau
if not (player ~= nil) then
	warn("Invalid player")
	return
end
```

Use `guard` at the top of methods. Happy-path code stays unindented.

## match — types and values

```clpp
match (instance) {
    Part* p => p.Anchored = true,
    Model* m => post(m.Name),
    string s => post(s),
    _ => warn("Instance not supported")
};
```

- `Part* p` → `x:IsA("Part")` then bind `p`.
- `string s` → `typeof(x) == "string"`.
- `_` is required as a fallback in real UI code so unknown instances do not silently drop.

Arms can be a block:

```clpp
match (tool) {
    Tool* t => {
        guard (t.Parent != null) else { return; }
        t::Activate();
    },
    _ => warn("not a tool")
};
```

Next: [Async, spawn, parallel](concurrency).
