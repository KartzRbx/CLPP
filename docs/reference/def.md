# def: data and type definitions

`def` is the canonical type-definition form. Its body or right-hand side tells you what it defines. See [Implementation status](status.md) for tested behavior and remaining compiler work.

## Data

```clp
def Stats {
    number Hp = 100;
    number Attack = 10;
}
Stats hero = Stats { Attack = 15 };
post(hero.Hp);
post(hero.Attack);
```

```output
100
15
```

A body creates a nominal type: its name matters, not only the shape of its fields. Initializers name fields with `Field = value`, without a leading dot; omitted fields use declared defaults. Objects preserve shared identity when passed into game logic. [Objects and inheritance](../guide/07-structs-and-inheritance.md) explains mutation.

## Alias

```clp
def Score = number;
Score score = 42;
post(score);
```

```output
42
```

An alias names a type expression. It does not create a new runtime constructor or an object identity. Aliases use the structure of their underlying type.

## Literal union

```clp
def Outcome = "ongoing" | "team_a" | "team_b";
Outcome result = "team_a";
post(result);
```

```output
team_a
```

Only listed alternatives are valid for this type. When a source value has a union type, every source alternative must be accepted by the destination. For example, `"idle" | "running"` can be assigned to `"idle" | "running" | "jumping"`, but the reverse assignment is rejected. Reordering alternatives does not change compatibility. A union can also flow into a single type when every alternative fits: `number | float` is accepted by `float`; `number | string` is not. These rules apply to variables, arguments and returned values and are checked by the current compiler.

## Callback

```clp
def DamageCtx { number Amount; }
def Modifier = (DamageCtx) -> void;
def Less<T> = (T, T) -> bool;
Modifier doubleDamage = func(DamageCtx context) -> void { context.Amount *= 2; };
Less<number> ascending = func(number left, number right) -> bool { return left < right; };
auto context = DamageCtx { Amount = 7 };
doubleDamage(context);
post(context.Amount);
post(ascending(1, 2));
```

```output
14
true
```

Parentheses list parameter types. `->` introduces the returned type. Generic aliases substitute `T` consistently. A callback value is written with an anonymous `func (...) -> Type { ... }`. [Functions](../guide/06-functions.md) explains use and checking.

## Generic data

```clp
def Box<T> { T Value; }
auto item = Box<number> { Value = 7 };
post(item.Value);
```

```output
7
```

The angle brackets declare type parameters. `Box<number>` is a specialization. A data specialization must supply exactly the number of type arguments declared by its definition. `Box<number, string>` is invalid for `def Box<T>`, and bare `Box` does not implicitly supply `T`. Unknown arguments are rejected in constructors and type annotations, including nested collection types. This is a type-level parameter, not a variable stored in the object. Mutable specializations retain their arguments: `Box<number>` cannot be assigned to `Box<float>` or `Box<string>`, and assigning it to a bare `Box` does not erase its type arguments. Equivalent aliases remain compatible. Field types substitute the concrete arguments recursively: in `Box<number>`, a `T` field is `number`, an `array<T>` field is `array<number>`, and a nested `Other<T>` field is `Other<number>`. Reads, writes, constructor arguments and instantiated defaults are checked against those types. This includes dictionary, optional and union fields. Explicit generic function signatures and inference from call arguments are implemented; methods on generic data definitions also preserve the receiver's arguments. For example, `T Get()` in `Box<T>` returns `number` when called on `Box<number>`. Nested results and method parameters follow the same substitution. Methods may declare independent parameters with `template`; see [Generic methods](../guide/06-functions.md#generic-methods). Single-base generic inheritance preserves base arguments, inherited fields and method signatures; see [Inheritance](../guide/07-structs-and-inheritance.md#inheritance).

## Inheritance

```clp
def Entity { number Hp = 100; }
def Boss : Entity { number Phase = 1; }
auto boss = Boss {};
post(boss.Hp);
post(boss.Phase);
```

```output
100
1
```

A colon names the bases. The derived definition retains members from one storage base and may implement multiple fieldless abstract contracts. Generic projections, compatible contract diamonds, interface views and checked override dispatch have compiler/VM tests. See [inheritance](../guide/07-structs-and-inheritance.md#inheritance).

## Modules without headers

Place definitions in ordinary `.clp` modules. Use `link` to establish dependencies and `export` to expose a declaration to importers. No header extension controls export. See [Modules](../guide/10-modules.md).
