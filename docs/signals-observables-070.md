# Signal, observable, and state (F12)

`signal<T>` in CL++ is a **Luau table signal**, not a cloned `BindableEvent`. Prefer Spark / Cluaupp `Signal` (see `stdlib/clpp/libs/spark.clh`).

`observable` maps to a ValueBase **only** when the value must replicate. Use `state<T>` (local table) when the value is local to a script.

Copying `BindableEvent` per signal is documented as the expensive fallback, not the default emit.
