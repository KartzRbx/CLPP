---
title: Async, spawn, parallel
---

# Async, spawn, parallel

## async / await

```clpp
async Data* FetchData(Player* player) {
    Data* data = await DataService:Server::WaitFor(player);
    return data;
}
```

`await` calls `__await`: Promises (`:expect()`) wait; anything else is returned as already-yielded.

## spawn

```clpp
spawn {
    task::wait(2);
    post("Delay finished!");
};
```

Emits `task.spawn(function() ... end)`.

## parallel

```clpp
parallel {
    ComputeComplexPhysics();
};
```

Emits `task.desynchronize()` / `task.synchronize()` for Parallel Luau actors. Keep Instance access on the synchronized side.

## pcall + destructure

```clpp
auto [ok, result] = pcall(func []() {
    return DataStore::GetAsync("PlayerData");
});
guard (ok) else {
    warn("store failed");
    return;
}
```

Next: [Server and client attributes](attributes).
