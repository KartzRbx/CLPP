# 12. Concurrency

The current runtime implements tasks, native threads, parallel calls, synchronization, coroutines, and isolated actors. These features affect execution order; choose one mechanism for a specific need and collect its results explicitly.

Named return-type-first functions, typed async callbacks, native threads and synchronization are VM-tested. Shared data identity and deep immutability are VM-tested. Direct and bound async methods preserve virtual dispatch and typed task results; see [implementation status](../reference/status.md).

## Async functions, spawn, and await

Mark a function `async`. `spawn function(arguments)` starts work and returns a typed task; `await taskValue` waits and reads its result. Awaiting an async call directly starts its pending task.

`task<T>` retains the result type, including generic and nominal data. Calling an async named function or callback creates a pending task. `spawn` and `await` schedule it on the current thread's cooperative executor. Runnable tasks receive bounded instruction turns; waiting tasks retain their frames until a dependency completes or an event fires. A task runs once and caches its result or failure for repeated observations. `parallel` schedules its pending arguments and collects their results in argument order. Native library calls retain their own blocking behavior. Use explicit `thread` for execution on another operating-system thread.

```clp
async number Compute(number value) { return value * 2; }
task<number> job = spawn Compute(21);
post(await job);
```
```output
42
```

## Async callbacks and captured state

An async function value has a callback signature returning a task. The lambda body still declares its eventual result after `->`; prefix the lambda with `async`.

```clp
def AsyncCounter = (number) -> task<number>;
AsyncCounter MakeCounter(number initial) {
  number total = initial;
  return async func(amount) -> number {
    total += amount;
    return total;
  };
}
auto counter = MakeCounter(10);
auto first = counter(2);
post(await first);
post(await spawn counter(3));
```

```text
12
15
```

Tasks retain their callback arguments and shared capture cells after the factory returns. Synchronize shared writes when starting concurrent work; the example awaits each write before starting the next. Named async values, explicit generic specializations, returned callbacks, fields and typed collections use the same task contract. Contextual parameter inference works from the expected callback signature. A synchronous function that returns a task executes immediately and returns that task; it is not itself rescheduled as an async function.

Use `await` or `spawn` for task-returning callbacks; `thread callback(...)` is rejected. Async callbacks do not fit synchronous event/observable handlers. `try`/`catch` can observe failures from both a pending task and a spawned worker; unobserved worker failures fail the run when execution drains its workers. Descendants retain actor host restrictions. Async virtual methods and bound method values use the same task contract. Extract a synchronous method value before using thread; direct thread object.Method(...) is rejected.

## Parallel calls

`parallel(callA(...), callB(...))` starts independent asynchronous calls, waits for all of them, and returns their results in argument order.

```clp
async number Double(number n) { return n * 2; }
auto results = parallel(Double(2), Double(5));
post(results[0]);
post(results[1]);
```
```output
4
10
```

## Native threads and join

`thread function(arguments)` runs a function on an operating-system thread. `join(handle)` waits for that thread and obtains its result. Use `join` to read a result and propagate the worker's error. Workers inherit the VM's `post` and `warn` streams; each output record is serialized within the execution.

```clp
number Work(number value) { return value; }
auto first = thread Work(20);
auto second = thread Work(22);
post(join(first) + join(second));
```
```output
42
```

### Program completion

The VM waits for every native thread and its descendants before the program finishes, even when their handles are not joined explicitly. `join` is still how you obtain a result and handle a worker failure at a chosen point.

```clp
number Finish(number value) {
  post("worker finished");
  return value;
}
auto worker = thread Finish(42);
```
```output
worker finished
```

An unobserved worker failure makes the execution fail after all workers finish. A failure received through `join` and caught by `try` is considered handled and is not raised again at completion. If the main program also fails, its error remains the primary diagnostic while the VM waits for the workers. Failures propagated from workers retain their source position.

## Atomics and mutexes

`atomic name = value;` creates an atomic handle. `atomic_load(handle)` reads it; `fetch_add(handle, amount)` performs an atomic increment. `mutex()` creates a lock handle; `lock` and `unlock` protect a compound operation. Pass these handles explicitly to workers. Handles belong to one program execution: independent VMs and subsequent runs have separate registries. Workers, awaited functions and actors within that execution share its synchronization context.

```clp
atomic count = 0;
auto gate = mutex();
number Increment(number counter, number gateHandle) {
  lock(gateHandle);
  fetch_add(counter, 1);
  unlock(gateHandle);
  return 1;
}
auto first = thread Increment(count, gate);
auto second = thread Increment(count, gate);
join(first);
join(second);
post(atomic_load(count));
```
```output
2
```

An atomic increment alone does not require the mutex shown here; the lock is useful when several operations must occur together. Ensure every successful `lock` reaches `unlock`, including recovery paths. Mutexes track their logical task owner, reject recursive acquisition and foreign unlocks, and release remaining owned locks when that execution context finishes. A cooperative task suspends on contention so another task can release the lock.

## Coroutines and deferred tasks

Create a coroutine with `coroutine.create(functionName)`. `coroutine.resume(handle)` resumes it; `coroutine.yield(value)` suspends it with a value. Coroutines coordinate suspension and resumption rather than creating a new native thread per resume. `task.defer(functionName)` schedules a callback for later execution in the current engine; the target callback typing contract is described in [functions](06-functions.md).

```clp
number Step() {
  coroutine.yield(1);
  return 2;
}
auto sequence = coroutine.create(Step);
post(coroutine.resume(sequence));
post(coroutine.resume(sequence));
```

```output
1
2
```

## Actors and host access

`actor(functionName, argument)` invokes a synchronous entry function with one checked argument and returns its declared result. The entry can await async work. Actor execution blocks host file-system, environment, and HTTP facilities. Descendant threads, awaited calls, parallel work and lazy tasks returned to the caller retain those host restrictions. The caller continues pumping runnable tasks while waiting for the actor's native worker.

```clp
number Twice(number n) { return n + n; }
post(actor(Twice, 21));
```

```output
42
```

Keep dependent work sequential. When sharing data, decide which worker owns it and which operations require synchronization. [Events](13-events.md) describe notification callbacks; connecting a handler does not itself create a thread.
