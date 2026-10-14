---
title: Definitions and the type system
description: Data shapes, aliases, unions, callback types, generics, inheritance, enums, methods, and visibility.
---

# Definitions and the type system

The complete examples below execute in the documentation suite and compare their displayed output. Remaining blocks marked target identify unresolved syntax or application contracts. CL++ officially targets its own bytecode VM. Luau/Roblox examples describe a possible transpiler written **in CL++**, not an official compiler backend. See [compiler status](../reference/status.md).

Programs describe players, items, attacks, and maps. `def` gives those concepts names. Read [Mental model](./02-modelo-mental.md) for the intended object identity rules.

`def` replaces the old data-declaration spelling in new source. Deprecation warnings and automatic migration still require verified compiler work; see [Reference](./14-referencia.md).

## One keyword, several type forms

| Form after the name | Meaning |
| --- | --- |
| `{ ... }` with fields | Data definition |
| `= Type` | Alias |
| `= A \| B` | Union |
| `= (A) -> R` | Function type |
| `<T>` before the body | Generic definition |
| `: Parent { ... }` | Inheritance |

A body creates a nominal data type; an assignment names a type expression. `def` groups these forms without importing C++'s separate type-definition keywords.

## Data fields and defaults

```clp
def Player { string Name; number Health=100; number Mana=30; }
auto player=Player { Name="Ana" }; post(player.Health); post(player.Mana);
```

```output
100
30
```

Each field has a type, name, and optional default value.

### Named initialization

```clp
def Player { string Name; number Health=100; }
Player ada=Player { Name="Ada" }; Player rui=Player { Name="Rui", Health=50 };
post(ada.Name); post(ada.Health); post(rui.Health);
```

```output
Ada
100
50
```

`Field = value` names an initializer field **without a leading dot**. Omitted fields use defaults. A required field without a default or supplied value must produce a target checking error.

### Instance access and identity

```clp
def Player { string Name; number Health=100; }
auto ada=Player { Name="Ada" }; Player alias=ada;
ada.Health-=10; post(ada.Name .: " has " .: ada.Health); post(alias.Health); post(alias==ada);
```

```output
Ada has 90
90
true
```

Member access **does** use a dot: `ada.Health`. Under the target identity model, `Player other = ada;` refers to the same object. Construct another value when you need an independent object. The VM implements this reference contract. `==` compares identity, and `const` freezes reachable data through every alias.

## Aliases

```clp
def Points=number; def PlayerId=number;
void Give(PlayerId player, Points amount) { post(player .: ": " .: amount); } Give(7,42);
```

```output
7: 42
```

Aliases add meaning to an existing type. They do not create distinct nominal integer types: `Points` and `PlayerId` remain compatible aliases of `number`. Distinct primitive wrappers are not established by this design.

## Unions and literal unions

```clp
def Success { number Value; } def Failure { string Reason; }
def Outcome=Success | Failure; def DirectionName="left" | "right";
Outcome result=Success { Value=42 }; DirectionName direction="left";
post(result==result); post(direction);
```

```output
true
left
```

A union accepts one of its alternatives; `Success` and `Failure` are project types. Literal unions restrict the actual permitted values.

```clp
def State="idle" | "walking" | "jumping";
void Animate(State state) { match(state) { "idle"=>post("idle"); "walking"=>post("walking"); "jumping"=>post("jumping"); } }
Animate("idle"); Animate("walking"); Animate("jumping");
```

```output
idle
walking
jumping
```

Adding `"falling"` should reveal incomplete matches. Literal parsing, exhaustiveness and invalid-case diagnostics have compiler tests.

## Function types

```clp
def Vec2 { number X; number Y; } def Listener=(number,Vec2)->void;
Listener listener=func(number value,Vec2 position)->void { post(value+position.X+position.Y); };
listener(3,Vec2 { X=4,Y=5 });
```

```output
12
```

This signature takes an integer and the application-defined `Vec2`, and returns no value. See [Functions](./06-funcoes.md).

## Generic definitions

```clp
def Box<T> { T Value; }
Box<number> count=Box<number> { Value=5 }; Box<string> message=Box<string> { Value="hello" }; post(count.Value); post(message.Value);
```

```output
5
hello
```

`T` is filled with a concrete type at use. Use generics for structures that genuinely work across several types, such as grids or stacks.

### Generic functions

```clp
template<typename T> optional<T> First(array<T> values) { guard(len(values)>0) else { return null; } return values[0]; }
post(First(array<number>(7,8))); post(First<number>(array<number>()));
```

```output
7
null
```

The guard handles an empty collection; a nonempty one yields its first element. Explicit calls such as `First<number>(array<number>(1, 2))` are compiler-tested, including the specialized optional return. The type argument can also be inferred: `First(array<number>(1, 2))` preserves the same `optional<number>` return. Missing or conflicting evidence requires explicit arguments.

### Constraints

```clp
def Scorable { number Points; } def Bonus:Scorable { number Multiplier; }
template<typename T:Scorable> number Sum(array<T> values) { number total=0; for(T value in values) { total+=value.Points; } return total; }
post(Sum(array<Bonus>(Bonus { Points=20,Multiplier=2 },Bonus { Points=22,Multiplier=3 })));
```

```output
42
```

Here `Scorable` is an application type with a `Points` field. `T : Scorable` is a nominal restriction: the argument must be Scorable or a compatible derived type. An unrelated definition with the same field names does not satisfy it. The compiler checks properties and method signatures through the bound, including nested data and collection iteration. Unknown members, incompatible writes, unsafe overrides and incompatible inherited member layouts are rejected. Numeric bounds prove every accepted alternative numeric. Intersections preserve all restrictions and expose the strongest compatible single-base member view; each branch still checks its layout and method signatures. A union is not proof that every alternative satisfies a stricter bound. One storage base plus fieldless abstract contracts, interface views and generic inherited projections are compiler/VM-tested.

## Inheritance and composition

```clp
def Vec2 { number X=0; number Y=0; }
def Entity { Vec2 Position; number Health=100; } def Mage:Entity { number Mana=50; }
auto mage=Mage { Position=Vec2 {} }; post(mage.Position.X); post(mage.Health); post(mage.Mana);
```

```output
0
100
50
```

A `Mage` is an `Entity` with an additional field. The target subtype relationship permits it where an `Entity` is expected:

```clp
def Vec2 { number X=0; number Y=0; }
def Entity { Vec2 Position; number Health=100; } def Mage:Entity { number Mana=50; }
void Heal(Entity entity,number amount) { entity.Health+=amount; }
Mage mage=Mage { Position=Vec2 {} }; Heal(mage,20); post(mage.Health);
```

```output
120
```

`Vec2` is an application definition whose fields must have suitable defaults for its empty initializer. Use inheritance for **is-a** relationships. Use a field for **has-a** relationships:

```clp
def Engine { bool Running=false; } def Car { Engine Engine; }
auto car=Car { Engine=Engine {} }; car.Engine.Running=true; post(car.Engine.Running);
```

```output
true
```

Prefer composition for ordinary reuse; deep inheritance makes initialization and behavior harder to follow.

## Nominal and structural types

Definitions with bodies are nominal: identical fields do not make differently named definitions interchangeable.

```clp error
def A { number X; }
def B { number X; }
A a = A { X = 1 };
B b = a; << Expected target type error: A is not B.
```

```output
type mismatch
```

Aliases and unions are interpreted through their underlying type expressions. Nominality does not make aliases such as `def Damage = number;` and `def Health = number;` distinct.

## Scoped enums

```clp
enum class Element { Fire, Water, Earth, Wind }
Element element=Element::Fire; post(element==Element::Fire);
```

```output
true
```

Use an enum for a closed set of named choices. `enum class` scopes names within the enum; `::` accesses the case. Exhaustive matching must be checked before it is advertised as implemented.

## Methods and the receiver

The supplied design declares a method outside its definition with `::`:

```clp
def Player { string Name; number Health = 100; }
void Player::Heal(number amount) {
    @this.Health += amount;
}
Player ada = Player { Name = "Ada" };
ada.Heal(10);
post(ada.Health);
```

```output
110
```

`@this` denotes the receiver, and instance calls use `.`. Declare the owning local definition before its external methods. Receiver fields follow the same type and private-access checks as methods written inside the definition; calls and extracted callbacks retain virtual dispatch. An external method on a generic `Box<T>` is written `T Box::Read()`, with `T` supplied by the receiver. A method may add independent template parameters, but cannot reuse an owner's parameter name.

### Public and private fields

```clp
def Safe {
    public string Owner;
    private number Code = 42;
    number Read() { return @this.Code; }
}
auto safe = Safe { Owner = "Ana" };
post(safe.Owner); post(safe.Read());
```

```output
Ana
42
```

The target rule restricts private fields to the definition's methods. Public/private modifiers precede individual members; private reads outside methods are rejected.

## Visibility between source modules

Types live beside functions in `.clp` source modules. There are no headers and no public/private rule based on a header extension.

```clp file=Entities.clp
<< Entities.clp
export def Player { string Name; number Health = 100; }
```

```clp
link "./Entities.clp" as Entities;
auto player = Entities.Player { Name = "Ada" };
post(player.Name);
post(player.Health);
```

```output
Ada
100
```

`link "./Entities.clp" as Entities;` connects the module. Only declarations marked `export` cross that boundary; other definitions stay internal. Paths resolve relative to the importing file. Qualified type identity and visibility enforcement still need compiler validation. See [Modules](./09-modulos.md).

## A user-written transpiler example

A transpiler written **in CL++** could represent a definition as Luau text:

```lua
-- Illustrative application output, not official CL++ compiler output.
type Player = { Name: string, Health: number }
-- The application would also generate field initialization.
```

Such an application must preserve field defaults and identity. For boolean defaults, a generated constructor should check `== nil` rather than use `or`: `false or true` would overwrite a supplied `false`. This is an application implementation concern; CL++'s official backend is its own VM.

## Type-form reference

| Need | Write |
| --- | --- |
| Data record | `def Player { string Name; }` |
| Default field | `number Health = 100;` |
| Alias | `def Points = number;` |
| Alternatives | `def E = "a" \| "b";` |
| Callback signature | `def F = (number) -> bool;` |
| Generic definition | `def Box<T> { T Value; }` |
| Inheritance | `def Mage : Entity { ... }` |
| Named choices | `enum class E { A, B }` |
| External method | `void Player::Heal(number n) { ... }` |
