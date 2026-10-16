---
title: Control flow
description: Conditions, loops, switch, match, guards, returns, and runtime recovery, explained separately.
---

# Control flow

The complete examples below execute in the documentation suite and compare their displayed output. Remaining blocks marked target identify unresolved syntax or application contracts. CL++ officially targets its own bytecode VM. Luau/Roblox examples describe a possible transpiler written **in CL++**, not an official compiler backend. See [compiler status](../reference/status.md).

Control flow decides what runs, how often, and in what order. CL++ uses conditions and loops, with `match` for closed alternatives and `guard` for early exits.

## if and else

```clp
number health=10;
if (health<=0) { post("game over"); } else if (health<20) { post("low health"); } else { post("healthy"); }
```

```output
low health
```

Parentheses around conditions and braces around bodies are required. `else if` tests another condition only when the earlier condition failed; `else` handles the remaining path.

## while and do/while

```clp
array<number> queue = {1, 2};
void Process(number value) { post(value); }
while (len(queue) > 0) { Process(pop(queue)); }

number attempts = 0;
bool connected = false;
do {
    attempts++;
    post(attempts);
} while (!connected and attempts < 3);
```

```output
2
1
1
2
3
```

`while` tests before running the body. `do/while` runs the body **at least once** and tests afterward. Its closing condition requires a semicolon. `continue` jumps to that condition; `break` leaves the loop without testing it. Locals declared inside the body do not escape into the closing condition.

## Counter loops

```clp
for (number i = 0; i < 10; i++) {
    post(i);
}
```

```output
0
1
2
3
4
5
6
7
8
9
```

The initializer runs once, the condition is tested before each iteration, and the update runs afterward. Use this form when you need an index or a specific step. Typed counter initialization is compiler-tested.

## Collection iteration

```clp
def Player { string Name; }
array<Player> players={Player { Name="Ana" }, Player { Name="Rui" }};
for (Player player in players) { post(player.Name); }
```

```output
Ana
Rui
```

Read this as “for each Player in players.” Prefer it when an index is unnecessary. The compiler retains the declared element type for member checks.

## break and continue

```clp
def Player { string Name; bool Alive; number Health; }
array<Player> players={Player { Name="skip", Alive=false, Health=1 }, Player { Name="Ana", Alive=true, Health=30 }, Player { Name="stop", Alive=true, Health=0 }, Player { Name="unreached", Alive=true, Health=50 }};
for (Player player in players) {
  if (!player.Alive) { continue; }
  if (player.Health==0) { break; }
  post(player.Name);
}
```

```output
Ana
```

`continue` moves to the next iteration; `break` exits the loop. Choose the conditions according to the rule being implemented: the first branch above skips a dead player before the second condition is reached.

## switch

```clp
number state=1;
switch(state) { case 0: post("idle"); break; case 1: post("walking"); break; default: post("unknown"); }
```

```output
walking
```

Use `switch` for simple independent values. For an enum or union with a closed set of alternatives, prefer an exhaustive `match`.

## Exhaustive match

```clp
enum class Direction { Up, Down, Left, Right }
string Name(Direction direction) {
 match(direction) { Direction::Up=>return "up"; Direction::Down=>return "down"; Direction::Left=>return "left"; Direction::Right=>return "right"; }
}
post(Name(Direction::Left)); post(Name(Direction::Right));
```

```output
left
right
```

The checker rejects non-exhaustive matches over finite alternatives, duplicate cases and unreachable arms. Omitting a required enum case reports `non-exhaustive match`; a detailed list of missing case names and the proposed `CLPP0501` identifier are not current diagnostic guarantees. Match arms use `=>`; the old delimiter is rejected.

### Matching literal unions

```clp
def Tile="." | "o" | "#" | " ";
number Points(Tile tile) { match(tile) { "."=>return 10; "o"=>return 50; "#"=>return 0; " "=>return 0; } }
post(Points(".")); post(Points("o")); post(Points("#")); post(Points(" "));
```

```output
10
50
0
0
```

Use `match` when the alternatives are closed. Use `if` for open conditions such as “health below 20.” Literal-union matching, exhaustiveness and invalid cases are compiler-tested.

## guard and early exits

```clp
def Target { number Health; }
void Attack(optional<Target> target) { guard(target!=null) else { return; } target.Health-=10; }
auto target=Target { Health=100 }; Attack(null); Attack(target); post(target.Health);
```

```output
90
```

Read this as “require a target; otherwise return.” After an exiting guard, the checker narrows the optional value to `Target`.

### Keeping the main path flat

```clp
def A { number Value; } def B { number Value; }
void DoWork(A a, B b) { post(a.Value+b.Value); }
void Nested(optional<A> a, optional<B> b) { if(a!=null) { if(b!=null) { DoWork(a,b); } } }
void Guarded(optional<A> a, optional<B> b) { guard(a!=null) else { return; } guard(b!=null) else { return; } DoWork(a,b); }
auto a=A { Value=20 }; auto b=B { Value=22 };
Nested(null,b); Guarded(a,null); Nested(a,b); Guarded(a,b);
```

```output
42
42
```

A guard's failing branch must leave the relevant path with `return`, `break`, or `continue`, so checking can narrow the type afterward. Exiting guards and optional narrowing are compiler-tested.

## return

```clp
number Double(number n) { return n*2; } post(Double(21));
```

```output
42
```

`return value;` leaves a function with its result. In a `void` function, use `return;` without a value.

## try and catch

```clp
void RiskyOperation() { report("unavailable"); }
try { RiskyOperation(); } catch(error) { post("failed: " .: error); }
```

```output
failed: unavailable
```

Use recovery around runtime operations that can legitimately fail. Compiler errors cannot be caught. The protected-call `:` form described in [Operators](./04-operadores.md) is a separate target contract from the VM's existing `pcall` operation. For expected absence, use `optional<T>` and a null check. The legacy Option/Result wrappers have been removed; use optional values and exceptions.

Network or Roblox DataStore examples require an application integration; they are not built-in promises of the CL++ VM.

## Choosing a construct

| Situation | Use |
| --- | --- |
| Open conditions | `if / else if / else` |
| Closed enum or union | `match` |
| Independent simple values | `switch` |
| Missing value means stop | `guard` |
| Each collection element | `for ... in` |
| An index or custom step | Counter `for` |
| Body must run at least once | `do/while` |
| Recoverable runtime failure | `try/catch`; protected calls where specified |
