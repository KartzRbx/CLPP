---
title: Reference
description: Glossary, symbols, target-syntax cheat sheet, implementation limits, and header-free migration to def.
---

# Reference

This summary describes the author's CL++ syntax. Generics and module state are
compiler-tested in the current checkout. Target examples remain future or
application-specific contracts and do not prove support in an installed release.
Official CL++ execution uses its own VM. A Luau transpiler would be an application
written in CL++, not an official backend. See [Implementation status](../reference/status.md).

## Glossary

| Term | Meaning |
|---|---|
| **alias** | Name for an existing type: `def Points = number;` |
| **attribute** | Contract marker, such as `@server` in the example transpiler input |
| **binder** | Analysis that resolves names and scopes |
| **checker** | Analysis of types and rules |
| **closure** | Function retaining shared cells from its creation scope; captured constants remain immutable |
| **context** | Server, client, or module category in an application's policy |
| **def** | Target keyword for data, aliases, unions, function types, and generics |
| **diagnostic** | Error, warning, or hint |
| **emission** | Artifact production; bytecode in the official compiler, target source in a transpiler application |
| **enum** | Set of named alternatives |
| **generic** | Type or function parameterized by types |
| **guard** | Condition with an early exit when it is not satisfied |
| **inheritance** | Relationship between a derived definition and a base: `: Base` |
| **IS-A** | Relationship used to assess whether inheritance is appropriate |
| **janitor** | Internal signal caretaker tracking managed subscriptions; no public janitor object |
| **lambda** | Anonymous function introduced by `func` |
| **link** | Source-module import |
| **match** | Selection among alternatives; exhaustiveness depends on types and implemented rules |
| **module** | `.clp` source file with reusable declarations; no header required |
| **narrowing** | Type refinement after checking presence or an alternative |
| **nominal** | Type identity determined by a definition rather than only its fields |
| **null** | Absence of a value; representation depends on the runtime |
| **optional** | Type with a present or absent value: `optional<T>` |
| **pragma** | Compilation directive at the start of a file |
| **prototype** | Function signature without a body; linkage requires a defined contract |
| **object reference** | Another name or argument can identify the same object; not a C++ pointer |
| **signal** | Event source with listeners; validate the particular API against the implementation |
| **template** | Proposed function type-parameter declaration form |
| **transpiler** | Application translating one source language into another |
| **union** | Type or literal alternatives: `A | B` |

## Symbols

| Symbol | Read as | Use |
|---|---|---|
| `.` | Member of | `ana.Health` accesses an object field |
| `::` | Scoped member | `Dir::Up` selects an enum alternative |
| `:` | Depends on the construct | Inheritance in `def B : A`; protected calls have a separate contract |
| `.:` | Join for display | `"health: " .: ana.Health` |
| `~>` | Managed connection | Cleanup registration in the example event contract |
| `->` | Returns | `(number) -> bool` in a function type |
| `=>` | Then | `match` branch |
| `@` | Attribute | `@server` in the example transpiler input |
| `?` | Optional or choice | `T?` in the type contract; `c ? a : b` in a ternary expression |
| `<T>` | Type parameter | Generic definitions and functions |
| `Field = value` | Initialize this field | Inside `Player { Name = "Ana" }`, **without a leading dot** |

The dot belongs to **access**: `ana.Health`. It does not precede a field name inside a named initializer.

## Cheat sheet

### Values and types

```clp
pragma strict;
const number MAX=10; number x=5; auto y=2.5; optional<number> maybe=null;
def Point { number X=0; number Y=0; } def Id=number; def Color="r" | "g" | "b"; def Predicate=(Point)->bool;
def Box<T> { T Value; } def Base { number Health=100; } def Derived:Base { number Extra=0; } enum class Dir { Up, Down }
Point p=Point { X=1,Y=2 }; p.X=3; Id id=MAX; Color color="g"; Predicate valid=func(Point value)->bool { return value.X>0; };
post(id); post(x); post(y); post(maybe); post(p.X); post(color); post(valid(p));
post(Box<number> { Value=7 }.Value); post(Derived {}.Health); post(Dir::Up==Dir::Up);
```

```output
10
5
2.5
null
3
g
true
7
100
true
```

`const` fixes the value. Typed declarations and `auto` represent variables in the target syntax; the compiler checks these declarations and their uses. `def` covers several type forms, which need not share identity or representation rules.

### Functions and decisions

```clp
enum class Dir { Up, Down }
number Add(number a,number b) { return a+b; }
def Operation=(number,number)->number; Operation add=Add;
auto identity=func(number value)->number { return value; };
void ShowDirection(Dir direction) { match(direction) { Dir::Up=>post("up"); Dir::Down=>post("down"); } }
void Count(bool allowed) { guard(allowed) else { return; } for(number i=0;i<3;i++) { post(i); } }
post(add(20,22)); post(identity(7)); ShowDirection(Dir::Down); Count(false); Count(true);
```

```output
42
7
down
0
1
2
```

Named functions begin with the return type. `func` introduces a lambda; `->` can appear in a lambda signature or function type. Preserve those arrows when migrating named functions.

### Expressions and iteration

```clp
def Point { number X; number Y; }
bool a=true; bool b=false;
post(a && !b); post(a || b); post(a ? "yes" : "no"); post("total: " .: 10);
array<Point> points=array<Point>(Point { X=1,Y=2 }); for(Point point in points) { post(point.X); }
```

```output
true
true
yes
total: 10
1
```

Typed loops, named initialization, synchronous callbacks and scoped-enum matching are tested in the local checkout. Application-specific event contracts and remaining module/generic cases require separate validation.

### Events and application contexts

```clp target
@server
void Save(Player player, number points);

players.PlayerAdded~>Connect(func (Player player) {
    post(player.Name);
});
```

This illustrates the **example transpiler's input**, not an official engine API. `Player` and `players` require application declarations and values. Cleanup and context checking must be implemented by that application. See [Contexts](10-contextos-e-seguranca.md) and [Events](11-eventos-e-concorrencia.md).

## Compare concrete workflows

Compare an actual implementation and workflow. Similar syntax does not imply the same runtime, and a proposed contract must not be compared as an available guarantee.

| Alternative | Useful evaluation questions |
|---|---|
| Luau | Does the application target an engine? Will you write a transpiler in CL++, or run directly on that engine? Which verified tools does the project require? |
| C++ | Does the project require native code or direct library integration? How does each implementation handle objects, memory, and external calls? |
| TypeScript and roblox-ts | Which packages, tools, and engine contracts are needed? Have the chosen versions and integrations been tested? |
| Rust | Which performance, object-identity, and concurrency requirements matter? Are there measurements and tests for the actual workload? |

Official CL++ execution uses its own VM. This manual makes no general claim that CL++ is faster, safer, or offers more checks than these alternatives. Their ecosystem and concurrency guarantees have not been audited here. See [Implementation benchmarks](../guide/16-performance.md) for historical measurements with a defined scope.

## Target-contract limits

- CL++ is not full C++: these examples do not promise `std::`, C++ macros, or pointer arithmetic.
- Modules use `link` and source files; **the language has no headers**.
- Target syntax uses types/`auto` for variables and `const` for fixed values. `let`, `var`, and `mut` are outside that syntax.
- `func` introduces lambdas; named functions place the return type before the name.
- JSX, C++ capture lists such as `[x]`, manual destruction with `delete`, and a complete effect system are not specified by these examples.
- Context attributes in an application's input language are not equivalent to a borrow checker or server-request validation.
- An application can generate another language, but this does not create an official target backend or engine integration for CL++.

Function-parameter defaults have been removed following the author-confirmed explicit-interface rule. Configuration defaults belong to fields of def options objects. Overloads and legacy named-call syntax still require cleanup; numeric conversions, lambdas, narrowing, function aliases and generics have separate implementation and validation contracts.

## Migration to `def`

The old `struct` keyword appears in parser history. This manual uses `def` for new programs. Migration includes construction and object semantics; neither automatic equivalence nor an implemented `CLPP0901` warning is established.

### Target form

```clp
def Player {
    string Name = "Ana";
    number Health = 100;
}

Player ana = Player { Name = "Ana", Health = 90 };
ana.Health = 80; post(ana.Health);
```

```output
80
```

### Manual review

1. Find old declarations and distinguish data, aliases, unions, and callbacks.
2. Write the appropriate `def` form; review defaults and generics separately.
3. Move named-function return types before names; preserve lambdas and function types.
4. Use `Type { Field = value }` for named initialization, without a leading dot. Preserve `object.Field` access.
5. Review object identity. `def`, arrays and dictionaries use shared references; construct independent objects explicitly when migrating code that relied on copies.
6. Keep declarations and imports in `.clp` source modules without introducing headers.
7. Once the compiler supports the required contracts, test construction, aliases, argument mutation, events, and application policies.

`clpp fmt` and `clpp fix --def` are proposed tools, not currently available commands. The verified CLI runs `clpp program.clp`, a REPL, and the language server. A transpiler written in CL++ must implement its own generation workflow. See [Errors and tools](12-erros-e-ferramentas.md).

## Continue learning

- [Getting started](01-comecando.md): the first file and tools.
- [Mental model](02-modelo-mental.md): how to read the language.
- [Definitions and types](07-def-e-tipos.md): the definition forms.
- [Modules](09-modulos.md): header-free organization.
- [Contexts and security](10-contextos-e-seguranca.md): application policies and their limits.
