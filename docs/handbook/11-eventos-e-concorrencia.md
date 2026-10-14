---
title: Events and concurrency
description: Signals, observables, managed connections, and the distinction between VM features and example host contracts.
---

# Events and concurrency

Games and tools react to changes: an entity appears, a control is clicked, or a value changes. Events separate the component producing a change from the components responding to it.

> **Contract boundaries:** CL++ officially runs on its own VM. Executable signal examples below are checked by the documentation suite. Remaining target examples preserve proposed host interfaces; engine services and `task::wait` require an application adapter. Managed connections use the event's internal caretaker, with no public janitor object.

## Signals

```clp
signal<number> onScore;

auto connection = onScore::Connect(func (number points) -> void {
    post("scored " .: points);
});

onScore::Fire(10);
```

The implemented interface declares a signal carrying a `number`, registers a listener and emits a value. `func` introduces the lambda; named functions place their return type before their name, as in `void Score(number points)`.

| Proposed operation | Purpose |
|---|---|
| `Connect` | Register a listener |
| `Fire` | Notify listeners |
| `Once` | Listen for only the next emission |
| `Wait` | Wait for an emission |

Handlers run synchronously in registration order and stop at the first error. Connect and Once return connection handles; Disconnect is idempotent. Once disconnects before invocation, including recursive emissions. Wait cooperatively suspends an async function and returns its typed payload. See the [event guide](../guide/13-events.md) for the tested notification, suspension and cleanup contract.

## Managed connections: `~>`

A connection can retain objects and callbacks. Its cleanup should follow the lifetime of the component that created it.

```clp
def Player { string Name; }
def Players { signal<Player> PlayerAdded = signal<Player>(); }
auto players = Players {};
players.PlayerAdded~>Connect(func (Player player) -> void {
    post("joined " .: player.Name);
});
players.PlayerAdded::Fire(Player { Name = "Ada" });
players.PlayerAdded::Clear();
players.PlayerAdded::Fire(Player { Name = "Lin" });
```

```output
joined Ada
```

The ~> form registers the connection with the event's internal cleanup manager. event::Clear() disconnects managed subscriptions, releases their callbacks and is safe to repeat. The source's word janitor describes this internal caretaker; it does not introduce a public janitor object or extra connection argument.

Player and Players are application declarations. The example's player notifications use the VM signal API; an external engine adapter still needs integration tests. Native resources release at execution completion. See [Transpilers and engine use cases](13-roblox-e-cluaupp.md).

## Observables

```clp
observable number health = 100;
void UpdateBar(number next) { post(next); }

auto subscription = health::OnChange(func (next) -> void {
    UpdateBar(next);
});

health = 80;
subscription::Disconnect();
health = 60;
```

```output
80
```

OnChange returns an ordinary connection with Connected and idempotent Disconnect. Notifications run after each assignment, including equal values, in subscription order and stop at the first error without undoing the assignment. New listeners wait until a later notification; disconnected listeners are skipped before their turn. UpdateBar is declared by the application. Managed ~>Connect/Clear remains specific to signals; native resources are released on execution completion; block-scope destructors are not defined.

## Asynchronous work, scheduling, and parallel execution

Waiting without blocking other work does not necessarily involve simultaneous CPU execution.

### `async` and `await`

```clp
<< A deterministic application adapter for this executable example.
async string FetchFromHost(string url) { return "response: " .: url; }
async string Fetch(string url) {
    return await FetchFromHost(url);
}

async void Load() {
    string response = await Fetch("address");
    post(response);
}
await Load();
```

```output
response: address
```

`FetchFromHost` is a deterministic application adapter in this test; it performs no network request. The intended `await` contract suspends this task until a result arrives. Scheduling, cancellation, and failure propagation must be defined by the implementation.

The current VM preserves `task<T>` payloads for async function values and `async func(parameters) -> T` closures. A call creates a pending task; `spawn` schedules it cooperatively, and `await` suspends the current task until its dependency is ready. Explicit thread starts a native worker. Capture cells survive factory returns, and `try`/`catch` can observe task failures. Cooperative event waits, dependency wakeups and cleanup on execution failure are tested. See the [concurrency guide](../guide/12-concurrency.md#async-callbacks-and-captured-state) for the tested contract.

### `spawn`

```clp
signal<number> ready;
async number Work() { return ready::Wait(); }
task<number> pending = spawn Work();
post("task scheduled");
ready::Fire(5);
post(await pending);
```

```output
task scheduled
5
```

`spawn` schedules the async call cooperatively. `Work` suspends on the event; the program prints the scheduling message, emits its payload and awaits the result. This uses no engine delay API and starts no native CPU thread. Use `thread` when native worker execution is required.

### Parallel execution

The supplied `parallel` and `@parallel` examples express a proposed execution policy. A transpiler or host implementing that policy must inspect shared effects, including those of called functions. The attribute itself provides no proven race-freedom guarantee. See [Contexts and security](10-contextos-e-seguranca.md).

## `destructure`

This declaration is a future proposal. The current language keeps typed tuple
values and indexed access; the author chose not to add destructuring now.

```clp target
destructure (name, health) = GetPlayerData();
```

The intent is to distribute parts of a result into separate variables. `GetPlayerData` belongs to the example. Arity, types, and declaration syntax need a validated language contract.

## Optional absence and exceptions

Use `optional<T>` and `null` when absence is a normal outcome. Use `report` and `try`/`catch` when a runtime operation fails. The former Option/Result constructors are removed.

```clp
number Divide(number a, number b) {
    guard (b != 0) else { report("division by zero"); }
    return a / b;
}
post(Divide(10, 2));
try { post(Divide(10, 0)); } catch (message) { post(message); }
```

```output
5
division by zero
```

| Situation | Model |
|---|---|
| Absence is normal | `optional<T>` and `null` |
| Runtime failure with a cause | `report`, `try` and `catch` |
| Several value alternatives | Union declared with `def` |
| Protected-call colon | Proposed syntax, not currently accepted |

Continue with [Types and variables](03-tipos-e-variaveis.md), [Functions](06-funcoes.md), and [Errors and tools](12-erros-e-ferramentas.md).
