# CL++ language contract and executable examples

Read this page before implementing a tool in CL++ or generating CL++ source.
It describes the current C++ VM language, not the historical compiler or the
independent Luau tool. Keep the language's spelling and semantics. A lexer token
alone does not establish a usable feature. Future proposals are listed separately.

The complete reference consists of this contract, the linked topic chapters,
[grammar](ebnf.md), [keyword inventory](keyword-inventory.md),
[implementation status](status.md) and [standard library](../guide/14-standard-library.md).
The repository contains runnable programs in `examples/language/`, expected outputs
in `examples/language/cases.json`, and application examples in `examples/capabilities/`.
`tests/examples/check_language_contract.py` checks the new programs, intentional
rejections and keyword coverage. It also detects new lexer keywords without a
documentation entry. Coverage is an omission check, not proof of all semantics.

## 1. File structure, tokens and comments

Use `.clp` source files. Execution starts with top-level statements; declaring
`main()` does not call it. Initializers run in source order after imported
dependencies. Declarations and executable statements can share a file. No headers,
preprocessor, implicit entry function or required object hierarchy is needed.

Names are case sensitive. Use ASCII letters or `_` to start a portable identifier,
then letters, digits or `_`. Whitespace separates tokens; braces define blocks.
Instructions and aliases end with `;`. Blocks and data/enum declarations end with
`}`; `do` ends with `while (condition);`. Comments use `<<` through the physical
line, or `<<[ ... ]>>` across lines. Block comments do not nest. Comment markers
inside quoted strings are text. C++ comment syntax is rejected.

An optional `pragma strict;` or `pragma nostrict;` occurs once at file start,
before links and declarations. Neither mode makes obsolete syntax valid.
See [lexical syntax](../guide/02-lexical-syntax.md), [lexer](lexer.md),
[comments](comments.md), [identifiers](identifiers.md) and [punctuation](punctuation.md).

## 2. Values and declarations

Forms: `Type name = expression;`, `auto name = expression;`,
`const Type name = expression;`, `const auto name = expression;` and
`constexpr Type name = expression;` (also `constexpr auto`). Explicit initialization
is required. Inference keeps one type; it does not allow later type changes.
`constexpr` accepts compiler-evaluable numeric expressions and literal strings,
booleans/null with compatible annotations; it does not evaluate arbitrary calls.
`decltype(expression) name = value;` selects the expression's type; the current
implementation makes this binding immutable, unlike an ordinary typed variable.

`number` is a signed 64-bit integer. `float` is floating point. `bool`, `string`
and `void` complete the primitive spelling. `void` is a result with no value.
`any` is an explicit dynamic type, not a substitute for an omitted annotation.
An integer can be used where a floating value is accepted; a narrowing numeric
conversion requires `static_cast<Type>(value)` and is checked for validity/range.
Integer arithmetic detects overflow; integer division truncates toward zero.

Literals include decimal/hex/binary integers (`42`, `0xFF`, `0b1010`), decimal
fractions/scientific notation (`3.5`, `1e3`), and digit separators (`10_000`).
Separators occur between digits. The signed integer boundaries are
`-9223372036854775808` and `9223372036854775807`.
Single/double quotes delimit strings, with escapes such as `\n`, `\t`, `\\`,
`\"`, `\'`. Backticks interpolate `${expression}`; `\${` escapes that marker.
`true`, `false` and `null` are distinct values. Hex literals remain integers;
editor color previews interpret `0xRRGGBB` and `0xTTRRGGBB`, where `TT` is transparency.
See [values/types](../guide/03-variables-and-types.md) and [literals](literals.md).
Executable: `examples/language/01_values.clp`.

### Type-first expressions

`(Type expression)` gives an expression an explicit checked type, for example
`(Outcome "ongoing")`. The checker verifies compatibility rather than trusting
an assertion or adding a runtime string-to-enum conversion. The operand runs once.
Incompatible literals, unrelated nominal data and unchecked dynamic call results
cannot be forced into a more specific type. Ordinary implicit numeric widening
is preserved; narrowing still requires a numeric `static_cast`.

```clp
def Outcome = "ongoing" | "team_a" | "team_b";
def Battle { Outcome Result; }
auto battle = Battle { Result = "ongoing" };
post((Outcome "team_a"));
if (battle.Result != (Outcome "ongoing")) { post("finished"); }
else { post("ongoing"); }
auto values = (array<number> { 1, 2 });
post(values[1]);
```

```output
team_a
ongoing
2
```

```clp error
def Outcome = "ongoing" | "team_a" | "team_b";
post((Outcome "invalid"));
```

```output
value does not satisfy explicit expression type
```

The form also supplies context to typed array initializers and callbacks, accepts
generic/module-qualified types, and retains normal grouping and constructors.
`(value - 1)`, `(Function(argument))` and `(Point { X = 1 })` remain ordinary
expressions. Use a space between the explicit type and its operand; `Type(value)`
is not this annotation. `::` keeps scoped-member/event access, so
`"ongoing" :: Outcome` is not the spelling of a typed expression.

## 3. Expressions, access and precedence

Arithmetic: `+ - * / %`; comparisons: `== != < <= > >=`; logic:
`and`/`&&`, `or`/`||`, `not`/`!`. Logical AND/OR short circuit. Bitwise operations
on integers: `& | ^ ~ shl shr`. `^` is XOR. `.:` concatenates display text;
`condition ? yes : no` selects one expression. `start .. end` builds an integer
range excluding `end`; the same notation between brackets selects a slice.

From tightest to loosest, the current parser groups: calls/member/index access;
prefix `- ! not ~ await spawn thread`; `* / %`; `+ - .: ..`; all comparisons;
`shl shr`; `&`; `^`; `|`; `and &&`; `or ||`; ternary. Binary operators at one
level associate left to right. Comparisons share one level rather than separating
equality from ordering. Ternary branches parse expressions. Write parentheses
around shifts before comparisons: `(1 shl 3) == 8`. This table records the actual
parser; do not assume C++/TypeScript precedence.

Assignment is a statement: `= += -= *= /= %=`. Prefix/postfix `++ --` update
supported mutable places. They are not generic chained-assignment expressions.
Use `object.Field`, `object.Method(args)`, `Module.Function(args)`,
`Enum::Case`, and `items[index]`. Methods receive `@this`; `self` is not an
implicit receiver. `::` also selects established native/event operations.
`->` denotes callback results; `=>` denotes match arms; `~>` denotes a managed
event connection. They are not interchangeable. See [operators](../guide/04-operators.md).
Executable: `examples/language/02_operators.clp`.

## 4. Control flow and failures

`if (condition) { ... } else if (...) { ... } else { ... }`,
`while (condition) { ... }`, `do { ... } while (condition);`,
`for (Type i = value; condition; step) { ... }` and
`for (Type item in collection) { ... }` require blocks. `auto` is permitted for
iteration inference. A `number` iteration source produces `0` through count-1.
Arrays yield values, dictionaries keys, strings Unicode characters.
`break;` exits the nearest applicable loop/switch; `continue;` advances the loop.
Do/while continue checks the closing condition; C-style for continue runs its step.

`switch (value) { case value: statements default: statements }` has no fallthrough.
`match (value) { pattern guard condition => action; _ => action; }` selects the
first matching arm. Guards are optional; an action may be a block. Scoped enum and
literal-union matches require coverage; guarded arms do not guarantee unconditional
coverage. Duplicate/unreachable cases are checked. `guard (condition) else { exit; }`
requires its failure path to exit and preserves proved facts on the remaining path.

`return value;` or `return;` leaves a function. `try { ... } catch (error) { ... }`
handles runtime failures; `catch { ... }` omits the message binding.
`report(message)` raises a failure, `warn(message)` emits a warning, and `post(value)`
writes output. Compile errors cannot be caught. `pcall(expression)` returns a
numeric success status (`0`/`1`), not a result payload. An uncaught error fails the run.
See [control flow](../guide/05-control-flow.md) and [errors](../guide/11-errors.md).
Executable: `examples/language/03_flow.clp`.

## 5. Collections, shared identity and immutability

Construct `array<T>(values...)` or a contextually typed `{ values... }` initializer.
Construct `dictionary<K,V>(key, value, ...)`; pairs alternate. Positions start at
zero. `len`, `push`, `pop`, `insert`, `remove`, `find`, `sort` and `list` are runtime
operations. Typed collections check elements/keys; mutable generic containers are
invariant. A missing dictionary key raises an error; writing a missing key inserts
it. Do not assume reads return `optional<V>`.

Strings use UTF-8 and character indexing. String/array slices exclude the end and
clamp bounds. Invalid element indexes fail. Containers and objects share identity
through assignment, parameters and fields; `==` compares their identity. Applying
`const` freezes reachable objects/collections through every alias. Slice/sort create
new containers while object elements retain their identity. Garbage collection
traces cycles. Concurrent compound mutations need explicit synchronization.
`Vector2/3/4` and `buffer::create` are built-in value APIs, not new declaration forms.
See [collections/text](../guide/09-collections-and-text.md) and the library chapter.
Executable: `examples/language/04_collections.clp`, `examples/vectors_buffers.clp`.

## 6. Definitions, fields, methods and inheritance

`def Name { Type Field; Type Other = default; Result Method(Type argument) { ... } }`
declares a nominal data type. Construct with `Name { Field = value }`, without
leading dots. Omitted fields use declared defaults; supply fields without defaults.
Defaults are checked in the definition's scope and evaluated per construction.
Positional data constructors and `new` are rejected. Assignment retains object identity.

`public`/`private` control fields/methods. `@this.Field` selects the receiver.
An external method uses `Result Name::Method(Type argument) { ... }` after the
local data declaration. Extract `object.Method` to retain a bound callback;
it evaluates the receiver once and preserves that object's virtual dispatch.

`def Child : Base, Contract { ... }` permits one storage base and multiple
fieldless contracts. `abstract def Contract { abstract Result Method(); }`
describes a nominal interface when it has no fields/concrete methods/storage bases.
Abstract data cannot be constructed. Concrete children implement all abstract
methods. `override` keeps signature, bounds, visibility and async mode;
`final` prevents inheritance/override as appropriate. Unrelated storage bases
and conflicting generic contract specializations are rejected. Generic bases map
owner parameters through inherited fields/methods. See [definitions](def.md) and
[inheritance](../guide/07-structs-and-inheritance.md).
Executable: `examples/language/05_objects.clp`, `examples/automation.clp`.

## 7. Aliases, enums, optionals and generics

`def Name = Type;` aliases a type. `def Outcome = "ongoing" | "team_a" | "team_b";`
is a literal union; `def Numeric = number | float;` is a type union. `&` composes
constraints/contracts. Aliases are not separate object identities.
`enum class State { Ready, Done }` supplies scoped cases such as `State::Ready`,
without payload syntax. `optional<T>` accepts `T` or `null`; null checks/exiting
guards narrow a binding where presence is proved. Writes/calls that invalidate
facts are handled by the checker. `tuple<A,B,...>` gives fixed typed positions
for event payloads; indexes must be constant integers within range.

`def Box<T> { T Value; }` and `def Getter<T> = () -> T;` parameterize data/aliases.
`template <typename T> T Identity(T value) { return value; }` declares a generic
function. Use `Identity<number>(42)` or infer from argument evidence.
Bounds use `typename T : Bound` and/or `where T: Bound, U: Other`; repeated bounds
combine. Constraints are nominal where data/contracts are involved. Generic bodies
must be valid for admitted types. Numeric union bounds support proved arithmetic.
Generic methods introduce their own distinct parameters after `template`.

Evidence from nested containers, aliases, inherited data and independent arguments
can infer type parameters. Ambiguous alternatives need explicit arguments;
return-only formals and a bare untyped lambda do not create evidence. Use explicit
arguments when storing a generic named function value, e.g. `Identity<number>`.
Formals from an enclosing generic function remain available in captured callbacks.
See [advanced types](../guide/08-enums-variants-option-result.md) and
[functions](../guide/06-functions.md) for inference and bounds in detail.
Executable: `examples/language/06_types_callbacks.clp`,
`examples/capabilities/02_generic_pipeline.clp`.

Optional fields omitted from an initializer use `null` when no explicit field default exists. Optional conditions and optional negation treat only `null` and boolean `false` as falsy; zero and empty strings are present. This rule applies to `if`, `guard`, loops, ternary conditions and short-circuit logical operands. Non-optional numeric conditions keep their existing behavior.

## 8. Functions and closures

Named functions use `Result Name(Type argument, Type other) { ... }`.
Arguments are explicit and positional. No parameter defaults, named call arguments
or overloaded declarations are supported. Use fields of an options `def` for defaults.
Function types use `(ParameterTypes) -> Result`; lambdas use
`func(Type parameter) -> Result { ... }`. The result annotation is mandatory.
An omitted lambda parameter type needs an unambiguous expected callback signature
from a declaration, call, return, field or typed collection. Callback signatures
are checked invariantly. A bare `auto f = func(value) -> number { ... };` cannot
infer the parameter from its body.

Closures capture used bindings by shared cells; they survive return and preserve
enclosing generic types. Siblings share mutations. For-in iterations create distinct
bindings; a C-style counter declared once remains one binding. A constant callback
binding can still mutate mutable captured state. Explicit worker concurrency
requires synchronization; capture does not create a worker.
See [functions](../guide/06-functions.md) for bound methods and all capture cases.
Executable: `examples/language/06_types_callbacks.clp`, `examples/capabilities/lib/calls.clp`.

## 9. Async, threads, coroutines and actors

`async Result Name(...) { ... }` creates a pending `task<Result>` when called.
`spawn call(...)` schedules it; `await task` schedules/waits and returns its result.
Tasks run once and cache result/failure. `parallel(callA(...), callB(...))` collects
results in argument order. Async callbacks use `async func(...) -> Result { ... }`,
and their callback type returns `task<Result>`. They are not synchronous event handlers.
The executor is cooperative; native calls can block it.

`thread Function(args)` runs a synchronous function/callback on an OS thread;
`join(handle)` retrieves its result/failure. For bound methods, extract the callback
first. Execution drains workers and descendants before resource cleanup, including
error paths; an unobserved worker error fails execution. `atomic name = initial;`,
`atomic_load`, `fetch_add`, `mutex`, `lock` and `unlock` synchronize shared work.
Handles belong to one execution. Avoid lock leaks; foreign/recursive locks are checked.

`coroutine.create(callback)`, `coroutine.resume(handle)` and `coroutine.yield(value)`
coordinate suspension. `task.defer(callback)` schedules deferred work. `actor(callback,
argument)` runs a checked synchronous entry with host restrictions retained by
descendants; filesystem/environment/network access is unavailable there.
See [concurrency](../guide/12-concurrency.md).
Executable: `examples/language/07_events_async.clp`, `examples/language/08_workers.clp`.

## 10. Events, cleanup and observables

`signal<T> event;`, `signal<A,B> event;` and `signal<> event;` declare typed payloads
(up to seven currently). `event::Connect(callback)` and `::Once(callback)` return
`connection`; `connection.Connected` observes status and `::Disconnect()` is idempotent.
`event~>Connect(callback)` registers with the event's internal cleanup manager;
`event::Clear()` disconnects managed registrations. There is no explicit janitor
argument/type. Ordinary connections and pending waits remain active after Clear.

`event::Fire(args)` runs synchronous void callbacks in registration order, stopping
on the first error. Each emission snapshots subscriptions; new handlers wait for a
later emission, disconnected handlers are skipped before their turn. Once disconnects
before invoking, even on error/recursive emissions. `event::Wait()` inside async
suspends until a future delivery: zero payloads return void, one returns T, multiple
return a typed tuple. Emissions are not buffered. Waiting with no runnable producer
or native worker reports an error instead of hanging indefinitely.

`observable Type name = initial;` (or `observable auto`) creates observed state.
`name.OnChange(callback)` returns an ordinary connection, called synchronously after
assignment; an error does not undo the assignment. Native resources are released at
execution completion after workers finish, not automatically at every block exit.
See [events](../guide/13-events.md).
Executable: `examples/language/07_events_async.clp`, `examples/capabilities/06_call_janitor.clp`.

## 11. Modules and native APIs

`link "./module.clp" as Module;` resolves relative to the importing file.
`link @clpp.json as Json;` selects a built-in module. `export` exposes definitions,
aliases, enums, typed functions and constants. Qualified module types retain nominal
identity. Nonexported names remain private. Normalized paths initialize once per run,
dependencies precede importers, cycles fail, and reading before initialization fails.
Imported aliases share module state. `::` is not a replacement for module `.` calls.

Use [modules](../guide/10-modules.md), [library](../guide/14-standard-library.md),
[console/files/JSON](../guide/19-audio-console-automation.md), and
[toolchain](toolchain.md) for API signatures and execution commands. Typed JSON uses
`Json.ParseValue`/`StringifyValue` with `Json.Value`, `Kind`, `Items`, `Entries`.
Ordinary record serialization is not automatically a JSON object with field names.
The [CLI host APIs](cli-host.md) describe processes, paths and independent stderr
in the current source build. Cluaupp port status is recorded in the repository's
`docs/internal/CLUAUPP_MIGRATION.md`; availability depends on the built compiler.
Executable: `examples/language/10_modules.clp`, `examples/capabilities/05_json_config.clp`.

## 12. Less common forms and limits

The VM supports `Result operator+(Left a, Right b)` and `operator-` for explicit
data operands. This is not overloaded ordinary function declaration support and
does not imply arbitrary custom operators. Although the parser recognizes
`operator[]` declarations, source indexing is not documented as dispatched through
them; do not generate such use without a complete semantic/runtime test.
The existing dynamic variadic form is `any First(any ... values) { return values[0]; }`.
It packs trailing values into the parameter; do not assume general typed variadic
generics or spread calls. Prefer typed arrays for a stable public tool API.
`extern number strlen(string value);` is the one supported external signature;
it is not a general C FFI. `move(local)` consumes a mutable local binding; constants
and shared file bindings cannot be moved. It is not object cloning.
Executable: `examples/language/09_advanced_forms.clp`.

Parser budgets reject excessive recursive syntax at 80 active entries and iterative
expression trees beyond 128 edges. Runtime resource limits are separate. These
limits and safe malformed-input handling are tested; they do not prove freedom
from all vulnerabilities. See [lexer limits](lexer.md).

## 13. Removed spellings and future proposals

Do not generate `struct`, `variant`, `namespace`, `import`, `from`, `using`, ignored
`static`, `new`, `cout`, `endl`, header paths, `[[attributes]]`, implicit `self`,
`@field`, `@this::field`, named `func Name`, bare/payload enums, untyped signals/tasks,
direct signal calls or `~>` match arms. `Option`/`Result`/`Some`/`None`/`Ok`/`Err` have
no intrinsic wrapper semantics. Ordinary user-defined names can have those spellings.
Legacy keywords may remain reserved to produce clear rejection messages.

Protected calls with `:`, destructuring and optional missing-key reads are
[future proposals](future-proposals.md), not current source forms. A C++ VM build
does not supply a CL++-to-Luau emitter, Luau source maps or a self-hosted compiler.
Use the migration backlog for those separate requirements. Never treat skipped
specification examples, editor parsing or successful highlighting as successful
compilation/execution.
