# 6. Functions and callbacks

CL++ writes an ordinary function's **return type before its name**. Parameters also use type before name. Braces contain the body; `return` exits with the result.

## Named functions

```clp
number Add(number left, number right) {
  return left + right;
}
void PrintScore(number score) {
  post("Score: " .: score);
}
PrintScore(Add(2, 3));
```

`number` promises an integer result; `void` means no result. Supply all arguments explicitly in parameter order. Parameter declarations with `= value` and named-call arguments such as `Read(value: 7)` are rejected. Put configuration defaults in fields of a `def` and pass an options object. Each function has one name in its owner or module: changing the parameter count does not permit another declaration with that name. Use distinct names for distinct operations.

```clp error
number Read() { return 1; }
number Read(number value) { return value; }
```

```output
already declared
```

## Recursion

A function can call itself. A terminating case prevents indefinite recursion.

```clp
number Factorial(number n) {
  if (n <= 1) { return 1; }
  return n * Factorial(n - 1);
}
post(Factorial(5)); << 120
```

## Function types

A `def` alias can describe a callback's parameter and result types. In a function type, parameter types appear in parentheses and `->` introduces the result. That arrow belongs to the callback type; ordinary named functions put the result first.

```clp
def Handler = (number) -> bool;
Handler IsPositive = func(number value) -> bool {
  return value > 0;
};
post(IsPositive(3)); << true
```

A stored callback is a value that can be passed, returned, placed in a field or collection, and invoked. The checker verifies its argument count, parameter types and result. Callback signatures are invariant; different signatures require an explicit wrapper.

## Passing callbacks

```clp
def Predicate = (number) -> bool;
bool Test(number value, Predicate check) {
  return check(value);
}
Predicate IsEven = func(number value) -> bool {
  return value % 2 == 0;
};
post(Test(8, IsEven)); << true
```

A lambda spells `func(parameters) -> ResultType { body }`. Its result type is explicit; the compiler rejects omitted `-> ResultType`, shorthand `x => expression`, and named declarations using `func Name(...)`. See [events](13-events.md) for callbacks used in notifications.

## Generic functions

A generic function introduces its type parameters before its return type and name. Type arguments can be inferred from values or supplied explicitly:

```clp
template <typename T>
T Identity(T value) {
  T copy = value;
  return copy;
}
number score = Identity(42); << T = number
string name = Identity("Ada"); << T = string
number explicitScore = Identity<number>(42);
post(score);
post(name);
```

The compiler substitutes the supplied type throughout parameters and results, including nested arrays, dictionaries, data types, optionals and unions. `Identity<number>("wrong")` and assigning its result to `string` are errors. The generic body is checked with a scoped formal type: returning an arbitrary integer as `T` is invalid. Omitting type arguments makes the compiler infer them from actual parameters, including `array<T>`, `dictionary<K, V>`, `Box<T>` and `optional<T>`. A direct string literal infers `string`. Repeated uses of a type parameter must agree; conflicting `number` and `float` evidence requires an explicit choice, such as `Choose<float>(1, 2.5)`. Parameters used only in the return type, absence without other evidence and dynamic values cannot supply a type. Inference through a formal union/intersection currently requires explicit arguments; choosing a branch is not guessed. Async results preserve specialization through `await`.

A nominal constraint such as `template <typename T : Scorable>` exposes the declared fields and methods of `Scorable` inside the function. Compatible derived arguments are checked against that contract, including method parameter/return types and execution mode. Matching field names alone are insufficient. A formal `T` can be passed to a function accepting its bound, while an arbitrary bound value cannot be returned as `T`. Numeric bounds permit operations proved valid for every admitted type.

Constraints can also follow the parameter list in a `where` clause. Repeated bounds on the same formal combine with each other and with its `template` bound. Member access uses the contract that guarantees that member, including composed fieldless interfaces. Calls validate every required bound; unrelated storage layouts cannot be combined by inheritance.

```clp
def Score { number Value; }
def BonusScore : Score { number Bonus; }
template <typename T : Score>
number Total(T value) where T: Score & BonusScore {
  return value.Value + value.Bonus;
}
post(Total(BonusScore { Value = 7, Bonus = 9 })); << 16
```

Every name before `:` must be one of the function's declared type parameters. Each call must satisfy every bound; a later `where` constraint never replaces an earlier one.

### Numeric bounds

```clp
template <typename T : number | float>
T Half(T value) { return value / 2; }
number whole = Half(7);
float fraction = Half(7.0);
post(whole);    << 3
post(fraction); << 3.5
```

A numeric bound guarantees that every admitted value supports the operation. The same formal type on both sides, or an integer operand combined with a numeric formal, preserves that formal's category. Calls still infer the real argument type: integer inputs stay `number`, decimal inputs stay `float`. A `float` bound follows numeric compatibility and admits integer arguments; inference retains the integer type. Use an explicit `<float>` argument when conversion is intended.

A bound containing `string`, such as `number | string`, does not guarantee arithmetic and is rejected for this body. Bit operations require an integer-only bound. Unconstrained formals cannot perform numeric operations. Different numeric formals without a proved shared result retain a `number | float` result instead of inventing one concrete type.

### Generic methods

A method can introduce its own type parameters with `template`. The data arguments come from the receiver; explicit call arguments belong to the method:

```clp
def Box<T> {
  T Value;
  template <typename U>
  U Echo(U value) { return value; }
}
Box<number> box = Box<number> { Value = 1 };
string name = box.Echo("Ada");
number score = box.Echo<number>(42);
post(name);
post(score);
```

Here `T` stays `number`, while each call chooses its own `U`. Owner and method parameter names must be distinct. Method bounds may refer to owner parameters. Calls through constructors, fields and indexes preserve the same checking. Generic overrides must keep the same signature and bounds after renaming their parameters. Async methods return task<T> results and preserve virtual dispatch; use spawn/await for their tasks. A formal type parameter does not identify a concrete constructor; constructing `Box<U>` uses the declared `Box` definition, while constructing an arbitrary `U` is not supported.

A generic callback alias preserves the concrete type used by the caller:

```clp
def Less<T> = (T, T) -> bool;
template <typename T>
bool Before(T left, T right, Less<T> compare) {
  return compare(left, right);
}
```

`T` stands for a concrete type chosen for the call. The compiler must substitute it consistently in parameters, callbacks, and the result. Generic data definitions use `def Box<T> { ... }`; see [definitions](07-structs-and-inheritance.md).

See [implementation status](../reference/status.md) for remaining callback inference, asynchronous callback and native interface work. [Modules](10-modules.md) explain functions shared between source files; [errors](11-errors.md) explain failures and diagnostics.

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

## Reference captures

A closure retains the bindings it actually uses. Captures share cells with the enclosing scope and sibling closures; they remain alive after the enclosing function returns.

```clp
def Counter = () -> number;
Counter CreateCounter(number start) {
  number value = start;
  return func() -> number { value += 1; return value; };
}
const auto next = CreateCounter(10);
post(next());
post(next());
```
```output
11
12
```

`const` prevents replacing `next`; calling it may still update its external captured state. Capturing a constant does not permit reassigning that constant. Captured data still respects deep freezing through all aliases. Each block declaration and each `for-in` iteration creates a distinct binding; a counter declared once in a C-style `for` retains one binding. Concurrent writes to captured bindings require explicit synchronization. The heap traces cycles between cells, callbacks and data, and callbacks retain their executable code. Callbacks from a different loaded executable are rejected.

Callbacks are synchronous. Use `thread callback(arguments)` for explicit worker execution. An observable's `OnChange` accepts `(ValueType) -> void`, runs after assignment and propagates handler errors. Repeated synchronous helper nesting is bounded to avoid overflowing the host stack; ordinary callback recursion uses VM call frames.

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

These callbacks can run in workers after the enclosing function has returned; synchronize shared writes explicitly. Lambda result annotations are still required. Async function values and async lambdas preserve task<T> results, captures and checked scheduling; see [concurrency](../guide/12-concurrency.md#async-callbacks-and-captured-state). Method values bind a checked receiver and retain virtual dispatch, including async methods.

### Contextual callback parameters

A lambda can omit a parameter type when its surrounding declaration, assignment, return or call supplies one unambiguous callback signature. Explicit parameter annotations are preserved and checked against that signature. Field initializers and defaults, typed collection constructors and observable handlers also supply context.

```clp
def Transform<T> = (T) -> T;
template <typename T>
T Apply(Transform<T> callback, T value) {
  return callback(value);
}
Transform<number> increment = func(value) -> number { return value + 1; };
post(increment(4));
post(Apply(func(value) -> number { return value + 2; }, 4));
```

```text
5
6
```

Generic calls use explicit type arguments or independent evidence from other arguments before checking an unannotated lambda, including arguments that follow it. Outer generic formals and their bounds remain in scope. A bare `auto callback = func(value) -> number { ... };` supplies no parameter context and is rejected. Conflicting annotations, wrong arity, ambiguous callback unions and missing generic evidence are also rejected; the checker does not infer a parameter from the lambda body or silently replace it with `any`.

### Function values with enclosing generic types

```clp
def Transform<T> = (T) -> T;
template <typename T>
T Identity(T value) { return value; }
template <typename U>
Transform<U> MakeIdentity() { return Identity<U>; }
auto integer = MakeIdentity<number>();
auto text = MakeIdentity<string>();
post(integer(7));
post(text("typed"));
```

```text
7
typed
```

The callback retains `U` until the enclosing call supplies its actual type. Formals belong to their declaration: a similarly named module type cannot replace them, and `Identity<U>` outside the declaration that introduces `U` is rejected. Explicit type arguments remain required for named generic function values. Their bounds are checked at the reference, even if the returned callback is never invoked.

### Bound method callbacks

Extract `object.Method` to retain a callback bound to that object. The receiver expression is evaluated once; later field changes remain visible, while reassigning the original variable does not retarget the callback. The callback's signature excludes the implicit receiver, accessed in the method body as `@this`. Virtual dispatch selects the actual object's implementation, including when the receiver is viewed through a base type.

```clp
def Counter {
  number Value;
  number Add(number amount) { @this.Value += amount; return @this.Value; }
  async number Read() { return @this.Value; }
}
auto counter = Counter { Value = 10 };
auto add = counter.Add;
auto read = counter.Read;
post(add(2));
counter = Counter { Value = 100 };
post(add(3));
post(await spawn read());
```

```text
12
15
15
```

Methods on generic data infer owner arguments from the receiver. A method's own generic arguments remain explicit in a stored value, such as `box.Echo<string>` or `box.Echo<T>` inside a declaration that introduces `T`. Access checks and bounds apply when extracting the value. An object's fields take precedence over a same-named method; a callback field keeps its own declared signature. A bare `Type.Method` does not supply a receiver and is rejected.

Bound callbacks retain the object through returns, collections, captures and worker use. Use `thread` on a stored synchronous callback and `spawn`/`await` on async callbacks. Async direct method calls also create pending tasks and dispatch virtually. Synchronous bound methods may serve as observable handlers; async methods do not fit a synchronous handler signature. See [concurrency](12-concurrency.md) for execution and synchronization rules.

## Inference through composed aliases

Aliases may combine a generic alternative with a concrete one:

```clp
def Choice<T> = T | string;
template <typename T>
T Keep(Choice<T> value, T fallback) { return fallback; }
post(Keep(7, 42));
post(Keep("ignored", 9));
```

```output
42
9
```

A concrete alternative contributes no generic evidence. In the second call,
`string` already admits the first argument; the fallback determines `T` as
`number`. Every possible alternative of a union argument must be covered, and
incompatible evidence is rejected. Arguments with unresolved composed choices
are retried after other arguments supply evidence, including the evidence used
to type contextual callbacks. Failed candidates cannot change another candidate.
If multiple generic branches still produce different choices, supply explicit
type arguments. An intersection alias checks
every contract and combines only consistent evidence. Mutable generic
containers retain their existing invariant element types.

Contextual parameter types come from the receiving callback signature. Generic
field callbacks with defaults are checked in their declaration scope; their
provisional signatures do not prevent unrelated event handlers or function
arguments from receiving their own contextual types. All bodies still undergo
final semantic checking before execution.
