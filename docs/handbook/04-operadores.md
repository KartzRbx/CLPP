# Operators

Member access, scoped enums, text construction and typed managed connections below are executable tests. Protected-call colon syntax remains a proposed contract. CL++ officially runs on its own VM. Roblox APIs and Luau emission appear only as examples of a transpiler application written in CL++, not an official backend.

## Arithmetic

| Operator | Purpose | Example |
| --- | --- | --- |
| `+`, `-`, `*`, `/` | Add, subtract, multiply, divide | `left * right + offset` |
| `%` | Remainder | `count % 2` |
| `++`, `--` | Increase or decrease by one | `index++` |
| `+=`, `-=`, `*=`, `/=`, `%=` | Calculate and assign | `health -= 10` |

`=` assigns an existing variable; it is not equality. Integer division in the current executable discards the fractional part. A floating-point operand enables fractional division. Keep computation numeric until text is needed for presentation.

## Comparison

`==` tests equality, `!=` inequality, and `<`, `>`, `<=`, `>=` compare ordered values.

For **objects defined with a `def` body**, the target equality contract compares object identity rather than every field. Read the [mental model](./02-modelo-mental.md). The VM implements shared identity and deep const immutability.

## Logical operators: two spellings

| Symbol spelling | Word spelling | Meaning |
| --- | --- | --- |
| `&&` | `and` | Both conditions must hold |
| `\|\|` | `or` | At least one condition must hold |
| `!` | `not` | Negate a condition |

```clp
bool alive=true; bool stunned=false;
if (alive && !stunned) { post("ready"); }
if (alive and not stunned) { post("ready"); }
```

```output
ready
ready
```

The design accommodates familiar spellings from different languages. The new handbook proposes normalizing toward `&&`, `||`, and `!` in a formatter; earlier author examples also use `and`, `or`, and `not`. Formatting preferences and editor hints are proposed tooling behavior, not proof that a `clpp fmt` command exists.

`and`/`&&` and `or`/`||` short circuit: the right operand runs only when needed. Use this to check a boundary before an indexed read, for example. Negation operates on one condition.

## Conditional expression

```clp
bool critical=true; number baseDamage=12;
number damage=critical ? baseDamage*2 : baseDamage; post(damage);
```

```output
24
```

Read it as: if the hit is critical, choose twice the base damage; otherwise choose the base damage. Use `condition ? yes : no` for **short value choices**. Use an `if` block when a branch needs several steps.

## Access operators and their separate roles

These operators distinguish instance access, scoped access, protection, text construction, and managed connections. Their spelling does not make engine services part of the language.

| Symbol | Intended role | Implementation status |
| --- | --- | --- |
| `.` | Instance field or method | Typed fields and methods are checked |
| `::` | Scoped/static member | Scoped enum identity and cases are checked |
| `:` | Protected call | Exact return and error behavior needs specification |
| `.:` | Concatenate values as text | Text concatenation exists in the current executable |
| `~>` | Managed connection in the supplied design | Internal signal caretaker; Clear disconnects managed subscriptions |

### `.`: instance access

```clp
def Player {
    string Name;
    void Kick(string reason) { post(reason); }
}
Player player = Player { Name = "Ada" };
string name = player.Name;
post(name);
player.Kick("goodbye");
```

```output
Ada
goodbye
```

Here `player` identifies a particular object; `Name` and `Kick` identify its members. This application supplies `Player` and its method; printing the reason demonstrates a checked method call, not a built-in Roblox service or player removal.

### `::`: scoped access

```clp
enum class Direction { Left, Right }
Direction facing=Direction::Right; post(facing==Direction::Right);
```

```output
true
```

Use `::` when selecting a scoped member of a type rather than a member of one instance. A host or transpiler project might also provide `task::wait(1)`; that API name is not evidence of an official engine integration. Existing native/library conventions should be checked separately from target scoped types.

### `:`: a protected call

This is a future proposal. The author chose to keep the current language; use
ordinary instance calls and explicit `try/catch` today.

```clp target
player:Kick("goodbye"); << Proposed protected-call form.
```

Use protection when a failure can be handled legitimately, such as an external operation or an object that may have become unavailable. The design must define the returned value, how an error is captured, and whether execution continues. Do not assume every failure is silently discarded.

A **user-written Luau transpiler** might implement protection through `pcall`. That mapping belongs to the transpiler; it is not the VM's language implementation. Current CL++ runtime error handling is explained separately in [Errors](./12-erros-e-ferramentas.md).

Do not confuse the colon here with inheritance (`Player : Entity`) or `? :` in a conditional expression. Ordinary declarations place the type before the name.

### `.:`: text concatenation

```clp
def Player { string Name; } auto player=Player { Name="Ana" };
string message="hello, " .: player.Name .: "!"; post(message);
```

```output
hello, Ana!
```

Use a dedicated text operator rather than assuming numeric `+` concatenates strings. The distinction avoids ambiguity when a message mixes text and numbers. A transpiler application targeting Luau could lower this to `..` with any conversions needed to preserve the intended behavior.

### `~>`: managed connections

```clp
def Player { string Name; }
def Players { signal<Player> PlayerAdded=signal<Player>(); }
auto players=Players {};
players.PlayerAdded~>Connect(func(Player player)->void { post("joined: " .: player.Name); });
players.PlayerAdded::Fire(Player { Name="Ana" });
players.PlayerAdded::Clear();
players.PlayerAdded::Fire(Player { Name="Rui" });
```

```output
joined: Ana
```

This host-specific illustration means: connect a callback when the event fires, and remember to clean up the connection. The definitions above provide Player and Players locally; PlayerAdded uses the VM's typed signal operations.

Connections can retain callbacks and references. The author's janitor is the signal's internal caretaker: ~>Connect registers a managed connection, and event::Clear() disconnects managed subscriptions. There is no public janitor object or extra argument. Typed VM signals implement this contract; the engine declarations in this example remain application-specific.

Ordinary event::Connect returns a connection whose ::Disconnect() operation releases its callback. Native window, graphics, interface and audio resources are released at the end of program execution after workers finish, including error paths. This does not introduce lexical block cleanup.

## A transpiler exercise: preserve access semantics

A developer writing a transpiler **in CL++** could consider the following output mappings. These are educational application choices, not official compiler guarantees:

| Input role | Possible Luau output |
| --- | --- |
| Instance property/method | `.` or `:`, according to the host signature |
| Static/scoped member | `.` on a suitable output representation |
| Protected call | A `pcall` wrapper with a defined result contract |
| Text concatenation | `..`, with required display conversion |
| Managed connection | An explicit connection plus a cleanup registration |

Correct emission requires type information and tests. Token replacement alone cannot determine whether a method needs a receiver, how failures propagate, or who owns cleanup.

## Named initializers: `Field = value`

```clp
def Point { number X = 0; number Y = 0; }
Point position = Point { X = 1, Y = 2 };
post(position.X);
```

```output
1
```

Each `Field =` names the field being initialized. **There is no leading dot in an initializer field.** Ordinary member access keeps its dot: `position.X`. Omitted fields use the defaults declared in the definition. Read the initializer as: a point whose X is one and whose Y is two.

Definitions, defaults and named initialization are exercised by the compiler/VM tests.

## Precedence and grouping

The new handbook intends multiplication/division before addition/subtraction, comparisons before logical AND, and logical AND before logical OR. Operator precedence and prefix grouping are covered by compiler tests.

Use parentheses to make the intended grouping clear: `(left + right) * scale`, `not (health <= 0)`, and `(flags & mask) != 0`. Do not infer every rule from C++.

## Quick reference

| Goal | Write |
| --- | --- |
| Read a field | `player.Name` |
| Select an enum case | `Direction::Right` |
| Express a protected call | `player:Kick()` in the target design |
| Join text | `"a" .: "b"` |
| Describe managed cleanup | `event~>Connect(callback)` in the target host model |
| Initialize named fields | `Point { X = 1 }` |
