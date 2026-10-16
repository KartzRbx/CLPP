---
title: Functions
description: Named functions, parameters, callbacks, lambdas, closures, recursion, and explicit function types.
---

# Functions

CL++ executes on its own VM. See [compiler status](../reference/status.md) for remaining module and advanced generic work.

A function names reusable behavior: describe it once and call it wherever it is needed.

## Declaring and calling functions

```clp
number Add(number a, number b) { return a + b; }
number total = Add(2, 3); << 5
```

The order is **return type, name, typed parameters, body**. Arguments supply values in parameter order. Use `void` when there is no result:

```clp
void Greet(string name) { post("hello, " .: name); }
```

## Source modules without headers

Write the complete function in its source module and connect the module with `link`. There is no separate header or prototype file.

```clp file=Combat.clp
<< Combat.clp
export number Damage(number strength, number defense) {
    return strength > defense ? strength - defense : 1;
}
```

```clp
<< Game.clp
link "./Combat.clp" as Combat;
post(Combat.Damage(20, 5));
```

```output
15
```

Mark public declarations with `export`; declarations without it are internal. Functions keep their return type first, after `export`. The compiler enforces this rule and initializes each imported module once per program run. See [Modules](./09-modulos.md).

## Explicit interfaces: no overloads or default arguments

The supplied design excludes function overloading and default function arguments. Give distinct operations distinct names, such as `PhysicalDamage` and `MagicDamage`, and supply required arguments explicitly. A data definition can hold options with **field defaults** instead:

```clp
def SpawnOptions {
    number Count = 1;
    float Delay = 0.0;
}
void Spawn(SpawnOptions options) { <<[ Application implementation. ]>> }
Spawn(SpawnOptions { Count = 5 });
```

Defaults belong to fields of `def`; they are not default function parameters. The compiler rejects parameter defaults in functions, methods and lambdas, including imported declarations. All function arguments must be supplied explicitly. Functions cannot share a name within the same module or owner, even with different parameter counts or generic declarations. Distinct modules and definitions may use the same member name.

## Functions as values

Functions can be stored, passed, returned and called through their checked signatures. `auto` retains the actual callback type.

### Lambdas

A lambda is an unnamed function value. Its typed spelling is `func (parameters) -> ResultType { body }`:

```clp
def Listener = (string) -> void;
Listener joined = func (string name) -> void {
    post("joined: " .: name);
};
joined("Ada");
```

A program can pass such a callback to an event API. The supplied Roblox event example is an application scenario for a user-written integration or transpiler, not an official CL++ backend or built-in event service.

### Explicit function types

Use `def` to name a callback's signature:

```clp
def Player { string Name; number Health; }
def Vec2 { number X; number Y; }
def Predicate = (Player) -> bool;
def OnScored = (number, Vec2) -> void;
array<Player> Filter(array<Player> players, Predicate keep) {
    array<Player> result = array<Player>();
    for (Player player in players) {
        if (keep(player)) { push(result, player); }
    }
    return result;
}
array<Player> allPlayers = {Player { Name="Ana", Health=100 }, Player { Name="Rui", Health=0 }};
auto alive = Filter(allPlayers, func (Player player) -> bool {
    return player.Health > 0;
});
post(len(alive)); post(alive[0].Name);
```

```output
1
Ana
```

The example supplies Player and allPlayers explicitly. `(Player) -> bool` describes one `Player` parameter and a boolean result. The checker rejects an incompatible callback at the call site. `CLPP0905` is a proposed diagnostic identifier, not a verified current diagnostic.

### Untyped callback escape

Lambda result annotations remain explicit. Parameter annotations may be omitted when a typed declaration, assignment, return, field/default, typed collection constructor, observable handler or call supplies one unambiguous callback signature. Generic call context uses explicit type arguments or independent evidence from the other arguments; outer formals and bounds are preserved. Explicit annotations are checked, never replaced. Missing or ambiguous evidence is rejected, including an unannotated parameter in a bare `auto` lambda. See the [function guide](../guide/06-functions.md#contextual-callback-parameters) for executable examples. The source design's untyped callback escape, proposed warning `CLPP0903` and quick fix still need implementation and tests. Its illustrative Luau type `(...any) -> any` is a possible output of a transpiler application, not CL++'s official compiler output.

## Closures

```clp
void Counter() {
    number n = 0;
    auto next = func () -> number {
        n++;
        return n;
    };
    post(next()); << 1
    post(next()); << 2
}
```

A closure captures the surrounding bindings it uses by reference. Both calls share `n`; its cell survives when the outer function returns. Captured constants remain immutable, and concurrent changes require explicit synchronization. C++ capture-list forms such as `[x]` and `[&]` are not part of this design.

## Recursion

A function can call itself. Provide a terminating case:

```clp
number Factorial(number n) {
    return n <= 1 ? 1 : n * Factorial(n - 1);
}
```

## Naming and scope

- Use verbs for operations: `Heal`, `Calculate`, `IsWall`.
- Use `Is`, `Has`, or `Can` for boolean questions.
- Keep a function focused on one operation.
- Declare parameter and result types explicitly; use `auto` for local inference, not an unspecified public signature.

## Generic functions as values

Supply explicit type arguments when storing a generic function. `Identity<number>` is a value with the signature `(number) -> number`; `Identity<string>` has a separate checked signature. `auto` preserves it, including the type returned by a callback.

```clp
 template <typename T>
 T Identity(T value) { return value; }
 def Transform<T> = (T) -> T;
 Transform<number> integer = Identity<number>;
 auto decimal = Identity<float>;
 auto text = Identity<string>;
 post(integer(42));
 post(decimal(7) / 2);
 post(text("typed"));
```

```output
42
3.5
typed
```

The same values can be passed, returned, stored in typed collections, captured by closures and invoked with `thread`. Relative modules expose them through explicit exports, for example `Module.Identity<Module.Player>`. Function bounds and nominal module identities are checked just as in a direct call. Missing or unknown type arguments, incompatible callback arguments/results and ambiguous overloaded references are rejected. Async function values return a checked task<T>; see [concurrency](../guide/12-concurrency.md#async-callbacks-and-captured-state). A method value such as `object.Read` binds the receiver object; see [bound methods](../guide/06-functions.md#bound-method-callbacks). Inside a generic function or method, an enclosing formal is also valid: `Identity<T>` keeps that declaration-scoped type in its callback signature. Any required bound must follow from the enclosing declaration; an unbounded formal cannot specialize a numeric-only function. Returned callback signatures become concrete when the enclosing function is called, including nested collections and captured callbacks.


## Closures in generic functions

A closure inherits the type parameters and bounds of its enclosing function. Captured values retain that declared type, and each call keeps its own capture cells alive after the enclosing function returns.

```clp
def Getter<T> = () -> T;
template <typename T>
Getter<T> Keep(T value) {
  return func() -> T { return value; };
}
auto integer = Keep(42);
auto text = Keep("typed");
post(integer());
post(text());
```

```output
42
typed
```

Here the results are `number` and `string`, respectively. Capturing an object retains its shared identity. Sibling and nested closures share the captured binding, so a write through one is visible to the others. Captured constants cannot be reassigned. Generic methods inherit both owner and method formals, and imported nominal types retain their module identities. Numeric/member operations inside a closure require the same bounds as in its enclosing generic function; no dynamic type is guessed.

These callbacks can run in workers after the enclosing function has returned; synchronize shared writes explicitly. Lambda result annotations remain required; parameter types can come from an unambiguous expected signature. Async function values and async lambdas preserve task<T> results, captures and checked scheduling; see [concurrency](../guide/12-concurrency.md#async-callbacks-and-captured-state). Method values bind a checked receiver and retain virtual dispatch, including async methods.
