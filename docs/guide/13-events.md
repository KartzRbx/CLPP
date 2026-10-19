# 13. Events and observable values

A signal carries a typed notification. An observable represents changing state. Both invoke synchronous callbacks in the caller's execution context; concurrency must be started explicitly.

## Typed signals and connections

**Candidate editor update (0.11.1, not yet published):** completion offers `Connect`, `Once`, `Fire`, `Clear` and `Wait` after `signal::`. Typing the first `:` also offers these beside `is<Type>`; acceptance inserts the second colon. `Connect` and `Once` insert callbacks with explicit payload types and editable argument names. Connections offer `::Disconnect()`, `::Connected()` and `.Connected`. Imported payloads keep their source spelling, such as `H.Human`, inside accepted callbacks. Signal fields, returned signals, parenthesized and indexed receivers, and generic parameters offer the same native operations. Callback member suggestions include accessible inherited fields and methods. See [release status](../reference/release-status.md) for the validated source and installation status.

Declare `signal<T> name;` for one payload, `signal<A,B>` for two, or `signal<>` for no payload. The current VM supports up to seven payloads. `Connect` retains a checked callback and returns a `connection` handle. `Fire` checks every payload against the declared types.

```clp
signal<number> damaged;
auto connection = damaged::Connect(func(number amount) -> void {
  post("Damage: " .: amount);
});
damaged::Fire(15);
post(connection.Connected);
connection::Disconnect();
connection::Disconnect();
damaged::Fire(20);
post(connection.Connected);
```

```text
Damage: 15
true
false
```

Callbacks run in registration order, stopping at the first thrown error. A `try`/`catch` around `Fire` can observe that error. Each emission snapshots its subscriptions: new listeners wait until a later emission, while listeners disconnected before their turn are skipped. Disconnecting does not cancel a callback that has already started. Mutating shared state across workers requires explicit synchronization.

`Once(callback)` creates an ordinary connection that disconnects before invoking its first callback, so recursive emissions cannot invoke it again. A thrown callback error still consumes a one-shot connection.

## Waiting inside async functions

`event::Wait()` suspends an async function until the next emission reaches its one-shot subscription. The task retains its frames, captures and error handlers; other runnable tasks continue on the executor. It returns `void` for no payload, the payload type for one value, or `tuple<A,B,...>` for multiple values. Tuple positions require constant integer indexes within their declared range, preserving each position's type.

```clp
async number Read(signal<number> event) {
  number value = event::Wait();
  return value + 1;
}
auto changed = signal<number>();
auto work = spawn Read(changed);
changed::Fire(41);
post(await work);
post(await work);
```

```output
42
42
```

```clp
async number Read(signal<number,string,bool> event) {
  tuple<number,string,bool> values = event::Wait();
  post(values[1]);
  post(values[2]);
  return values[0];
}
auto changed = signal<number,string,bool>();
auto work = spawn Read(changed);
changed::Fire(23, "ready", true);
post(await work);
```

```output
ready
true
23
```

Calling `Wait` from a synchronous function or module code is an error. Emissions before registration are not buffered. The wait participates in registration order: an earlier handler failure prevents delivery to later subscriptions. `Clear()` disconnects managed callbacks and leaves pending waits active. Completion or failure of execution disconnects remaining wait subscriptions. If every task is suspended and there is no runnable producer or active native worker, execution reports `async tasks are waiting with no event producer` instead of hanging indefinitely. A native thread can emit a signal to resume a waiting task.

## Managed subscriptions: `~>`

`event~>Connect(callback)` registers the connection with the event's internal cleanup manager. There is no public janitor object or extra janitor argument. `event::Clear()` disconnects these managed connections and releases their retained callbacks; repeated clearing is safe, and new managed subscriptions can be added afterward.

```clp
signal<number> changed;
auto managed = changed~>Connect(func(value) -> void { post(value); });
changed::Fire(7);
changed::Clear();
changed::Clear();
changed::Fire(9);
post(managed.Connected);
```

```text
7
false
```

Ordinary `::Connect` connections use their handle's `::Disconnect()` operation. `::Clear()` operates on the managed subscriptions. Disconnected records are pruned on registration, emission and managed clearing. Connections preserve returned closure captures, bound method receivers, module identities and generic payload types. Handlers must return `void`; async callbacks are rejected. Refer to [functions](06-functions.md) for callback types and parameter inference.

## Observable values

Write `observable Type name = initialValue;`. `name.OnChange(callback)` registers a callback receiving the new value and returns a `connection`. Assign to the name normally; the callback runs after assignment. Retain the handle to inspect `Connected` or call `::Disconnect()`; repeated disconnection is safe.

```clp
observable number coins = 10;
auto subscription = coins.OnChange(func(value) -> void {
  post("Coins: " .: value);
});
coins = 25;
coins = 30;
subscription::Disconnect();
coins = 40;
post(subscription.Connected);
```

```output
Coins: 25
Coins: 30
false
```

The initial declaration does not invoke subsequently registered callbacks. Notifications run synchronously in registration order and snapshot their connections: newly added handlers wait until a later assignment, and disconnected handlers are skipped before their turn. A thrown error stops notification without undoing the assignment. Disconnect releases the callback's captures and bound receiver. Discarding the handle leaves the subscription active while its observable remains reachable. Observable `OnChange` creates ordinary connections; the signal's managed `~>Connect`/`Clear` API remains specific to signals. Native resources are released at execution completion after workers finish, including error paths; block-scope destructors are not defined.

Use direct calls for one known operation, signals for several subscribers, and observables for state changes. Connecting a handler does not create a worker; see [concurrency](12-concurrency.md). Source modules can expose typed signals through exported functions; see [modules](10-modules.md).
