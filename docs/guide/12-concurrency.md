# 12. Concurrency

CL++ provides asynchronous functions, tasks, native threads, parallel expressions, atomics, mutexes, coroutines, and isolated actors. Choose the smallest mechanism that fits the work.

`async func` creates an asynchronous function; `await` waits for its result. `spawn` starts a task and `join` waits for one. `parallel(...)` evaluates independent operations concurrently. `thread` creates a native thread. Shared counters can use `atomic`, `atomic_load`, and `fetch_add`; compound state can be protected with `mutex`, `lock`, and `unlock`.

Coroutines can suspend and resume a sequence of operations. `actor(...)` isolates work from the file system, environment, and HTTP host facilities. This is useful when executing less trusted logic within a game or tool.

```clp
async func compute() -> int {
  return 42;
}
post(await compute());
```

Concurrency changes execution order. Coordinate tasks when output ordering or shared state matters; use a single task for simple sequential work. [Events](13-events.md) describes `signal` and `observable` for reacting to changes.

## Choosing a mechanism

| Need | Tool |
| --- | --- |
| Wait for an asynchronous result | `async` and `await` |
| Start work and join it later | `spawn` and `join` |
| Run several independent computations together | `parallel(...)` |
| Use an operating-system thread | `thread` |
| Protect a shared counter | `atomic`, `fetch_add` |
| Protect several related shared values | `mutex`, `lock`, `unlock` |
| Isolate code from host file, environment, and HTTP access | `actor(...)` |

Concurrency does not make sequential dependent steps faster. Avoid shared mutable state when values can be copied into independent tasks and results combined afterward. When sharing is necessary, define ownership and synchronization before adding threads.
