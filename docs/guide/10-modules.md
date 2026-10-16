# 10. Modules and link

CL++ connects source modules directly. **There are no header files:** declarations and implementations belong in the same source modules. `link` connects your program to another source file or a built-in module.

## Built-in modules

```clp
link @clpp.axiom as Axiom;
post(Axiom.Clamp(150, 0, 100));
```
```output
100
```

`as Axiom` names the alias. The current library exposes qualified calls through that alias. Its established API spelling must be checked separately from redesigned static type member access (`Type::Member`).

## Source modules

```clp file=score.clp
const number BONUS = 5;
export number AddBonus(number points) { return points + BONUS; }
```

```clp
link "./score.clp" as Score;
post(Score.AddBonus(10));
```

The function and constant are declared in `score.clp`; the entry file imports and calls them. Constants put `const` before the type. No companion declaration file is needed.

`export` marks declarations that callers can use through the alias. Without it, declarations stay internal: `BONUS` is available to `AddBonus`, but callers cannot use `Score.BONUS`. Qualified types have distinct module identities. The compiler enforces exports for functions, definitions, aliases, scoped enums and constants. Completion through the module alias shows only exports.

## Types across module boundaries

Expose data definitions, aliases, scoped enums, functions, and constants explicitly:

```clp file=entities.clp
export def Player { string Name; number Health = 100; }
export const number MAX_HEALTH = 100;
export number StartingHealth() { return MAX_HEALTH; }
```

Callers refer to exports through their module alias. Module-local declarations cannot be accessed by another module. Exported functions can use internal helpers without exposing those helpers.

## Relative path resolution

Paths resolve relative to the file containing the link. A file at `src/core/game.clp` links to `src/entities/ghost.clp` using `link "../entities/ghost.clp" as Ghost;`. Moving the entry file does not change the meaning of that link. Nested imports and normalized-path cycle detection are covered by compiler tests.

Static links initialize dependencies before their importers, before the entry statements execute. A normalized module path initializes only once per program run, even when linked under several aliases or through several dependencies. Cyclic imports are errors. An initializer failure stops execution before the entry file runs. Reading a binding before its initializer executes raises an error.

## Library navigation

| Purpose | Modules |
| --- | --- |
| Math and text | `@clpp.axiom`, `@clpp.math`, `@clpp.text` |
| Files, environment, URLs | `@clpp.fs`, `@clpp.os`, `@clpp.http` |
| Window, drawing, interfaces | `@clpp.window`, `@clpp.gfx`, `@clpp.ui`, `@clpp.gui` |
| Audio and input/output | `@clpp.audio`, `@clpp.io`, `@clpp.input` |
| Data and timing | `@clpp.json`, `@clpp.time` |

See the [standard library](14-standard-library.md) and [modular combat](20-modular-combat.md). Module APIs do not replace the explanations of [definitions](07-structs-and-inheritance.md), [functions](06-functions.md), or [lexical syntax](02-lexical-syntax.md).

## Internal state and initialization

A module can retain private variables and execute setup code. Give state variables explicit initial values. Module state belongs to the running program, not to a process-global registry; rerunning the VM creates fresh state.

```clp file=counter.clp
number count = 0;
export number Next() { count += 1; return count; }
```

```clp
link "./counter.clp" as First;
link "./sub/../counter.clp" as Second;
post(First.Next());
post(Second.Next());
```
```output
1
2
```

The entry file follows the same file-scope rules: its functions access its top-level bindings, and entry initializers execute in source order after dependencies. Each file binding requires an explicit initializer.

Both aliases share the same binding. Functions and returned closures can retain access to private state. Workers and actors share that state with their execution context; synchronize compound updates explicitly. `const` fixes its binding and freezes reachable data through aliases. Exported constants may use computed initializers, not just literals. Destructive `move` from a shared module binding is rejected; shared state requires a separate ownership contract. Observable state uses the same typed `OnChange` rules as local observables.
