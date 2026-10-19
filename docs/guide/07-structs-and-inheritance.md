# 7. Definitions, objects, and inheritance

`def` unifies data definitions and type aliases. A body defines a nominal type with fields; `= Type;` defines an alias. The compiler rejects the old `struct` spelling.

`def`, typed fields, defaults, named initialization and single-base generic inheritance are checked by the compiler. Objects retain shared identity through assignment, fields, parameters and worker results. The managed heap collects unreachable cycles. Applying `const` freezes reachable data through every alias.

## Data definitions and field defaults

```clp
def Player {
  string Name = "Ada";
  number Health = 100;
}
auto player = Player {};
post(player.Name);
post(player.Health);
```

```output
Ada
100
```

Fields use `Type Name = default;`. Defaults belong to fields; they are not default function arguments. A data definition creates a distinct named type, even when another definition has fields with identical types.

## Methods and instance access

Use `object.Member` for instance access. Functions receive typed objects and can work with their members.

```clp
def Fighter {
  number Health = 100;
}
void ApplyDamage(Fighter target, number amount) {
  target.Health = target.Health - amount;
}
auto hero = Fighter {};
auto alias = hero;
ApplyDamage(alias, 20);
post(hero.Health);
post(alias == hero);
```
```output
80
true
```

Changes through a `Fighter` argument affect the caller's object. Methods use the same reference model and access their receiver through `@this`. `==` compares object identity rather than recursively comparing fields.

A method can also be defined after its owning local `def`, using `Type::Method` and the receiver expression `@this`:

```clp
def Fighter { number Health = 100; }
void Fighter::Heal(number amount) { @this.Health += amount; }
number Fighter::Read() { return @this.Health; }
auto hero = Fighter {};
hero.Heal(10);
auto read = hero.Read;
post(read());
```

```output
110
```

External methods have the same receiver identity, access checking, generic specialization and virtual dispatch as methods inside the definition. An imported definition's methods belong to its defining module; it cannot be extended by declaring a qualified foreign method in the importer.

## Named initialization

```clp
def Fighter { string Name; number Health = 100; }
Fighter hero = Fighter { Name = "Ada" };
post(hero.Health); << Intended result: 100.
```

```output
100
```

Write the type, braces, and `Field = value` entries separated by commas, without a dot before the field name. An omitted field uses its declared default. An `optional<T>` field without an explicit default becomes `null`; other required fields without a default must be supplied. The compiler rejects unknown fields, duplicate entries and incompatible initializer types.

In the editor, completion inside the braces offers accessible fields from that definition and its bases, including imported definitions. Fields already supplied are excluded. Generic field types use the constructor's concrete type arguments. Hover and Go to Definition on an initializer key resolve to its declaring field; keys receive property highlighting. Suggestions also work while a field name is unfinished.

## Aliases

```clp
def Player { string Name; }
def PlayerId = number;
def Tile = "." | "o" | "#" | " ";
def Handler = (Player, number) -> bool;
PlayerId id = 7;
Tile tile = "#";
Handler healthy = func(Player player, number minimum) -> bool { return len(player.Name) >= minimum; };
post(id);
post(tile);
post(healthy(Player { Name = "Ada" }, 3));
```

```output
7
#
true
```

An alias names an existing type expression without creating a new nominal identity. An alias of a data definition retains that definition's identity and can be used both in annotations and named construction, including imported definitions and concrete generic types. A literal union limits accepted values to the listed literals. Function aliases describe callback arguments and their result. See [alternatives](08-enums-variants-option-result.md) and [functions](06-functions.md).

```clp file=Wallet.clp
export def Account {
  number Amount = 0;
  number Grant(number amount) { @this.Amount += amount; return @this.Amount; }
}
```

```clp
link "./Wallet.clp" as Wallet;
def _Wallet = Wallet.Account;
_Wallet account = _Wallet {};
post(account.Grant(500));
```

```output
500
```

`_Wallet` and `Wallet.Account` are two names for the same type. To create a distinct data type, define another `def` with a body; a type alias does not copy or recreate its definition.

## Generic data definitions

```clp
def Box<T> { T Value; }
auto integer = Box<number> { Value = 42 };
auto text = Box<string> { Value = "typed" };
post(integer.Value);
post(text.Value);
```

```output
42
typed
```

`T` is a type parameter, not a runtime variable. `Box<number>` specializes the field to an integer. Construction, nested field types, method results and arguments retain the specialization. Mutable specializations are invariant: `Box<number>` cannot be assigned to `Box<float>`.

Generic field defaults retain the declaring type's scope, including inherited defaults. Collection constructors, signals, contextual lambdas and named function values can use that type's formals. Each construction evaluates its omitted defaults, creating fresh collections and signals; defaults refer to declaration globals rather than the caller's local names.

```clp
def Transform<T> = (T) -> T;
def Defaults<T> {
  array<T> Items = array<T>();
  Transform<T> Apply = func(value) -> T { return value; };
  signal<T> Changed = signal<T>();
}
auto first = Defaults<number> {};
auto second = Defaults<number> {};
push(first.Items, 7);
post(first.Apply(first.Items[0]));
post(len(second.Items));
```

```output
7
0
```

Unknown names/types, recursive defaults and invalid callback bodies fail even when the definition is unused. A default assigned to an unconstrained `T` is checked against the actual field type when it is used: `T Value = 1` permits an omitted integer or float value, but a string specialization must supply a compatible explicit initializer. The declaration's generic callback body must itself be valid for its formal types; constructing only one specialization does not relax that body check.

## Inheritance

```clp
def Entity { number Health = 100; }
def Boss : Entity { number Phase = 1; }
auto boss = Boss {};
Entity view = boss;
view.Health -= 20;
post(boss.Health);
post(boss.Phase);
```

```output
80
1
```

The colon names the base definition. The child adds fields to the entity model. Use inheritance for a real subtype relationship; compose definitions through fields for ordinary reuse. A definition may inherit one storage base and multiple fieldless contracts. The base can appear anywhere in the list; inherited storage retains its field positions. Two storage bases are rejected, including a diamond that would merge their fields.

### Fieldless contracts

An `abstract def` with no fields, only abstract methods, and only fieldless contract bases is an interface contract. Abstract method signatures end with `;`. A concrete child must provide each method, either directly or through its storage base. All contract methods are public and overridable. An override must preserve argument/result types, generic constraints and async mode, and cannot reduce visibility.

```clp
abstract def Named { abstract string Name(); }
abstract def Scored { abstract number Score(); }
def Storage { number Value; number Score() { return @this.Value; } }
def Player : Named, Storage, Scored {
  string Name() { return "Ada"; }
}
auto player = Player { Value = 17 };
Named named = player;
Scored scored = player;
post(named.Name());
post(scored.Score());
```

```output
Ada
17
```

Contract views preserve the original object's identity and dispatch to its concrete implementation. Extracting a method callback retains the receiver. Async contract methods use `abstract async` and produce the same typed tasks as their concrete implementations. A fieldless abstract definition with a concrete method is an abstract class, not an interface contract. Abstract definitions cannot be constructed; matching method names without declared inheritance does not satisfy a nominal contract.

Generic contracts retain their type arguments through every inheritance path. A diamond may repeat the same contract specialization; inheriting both `Readable<number>` and `Readable<string>` is rejected even when the child remains abstract. Constraints may combine unrelated fieldless contracts with `where T: Named, T: Scored`, exposing the members guaranteed by each contract.

### Generic bases

```clp
def Base<T> {
  T Value;
  T Get() { return @this.Value; }
}
def Named<U> : Base<U> { string Name; }
template <typename T> T Read(Base<T> value) { return value.Get(); }
auto item = Named<number> { Value = 42, Name = "answer" };
Base<number> view = item;
post(view.Get()); << 42
post(Read(item)); << T is inferred as number.
```

A child supplies each base's full type arguments. Nested mappings such as `Base<array<U>>` substitute recursively. Inherited fields, methods and generic call inference use that mapping; `Named<number>` is a subtype of `Base<number>`, but not `Base<float>`. Overrides must preserve the specialized method signature. Imported bases and aliases follow the same rules. Cyclic inheritance is rejected. Generic inheritance follows the same one-storage-base rule and supports multiple fieldless contracts.

## Files and migration

Definitions live in source modules; CL++ has no header files. Use [link](10-modules.md) to connect modules. Replace old data declaration spelling with `def`, then review construction, aliases, and object mutation separately. Review old code that relied on value copies: aliases now refer to the same object.
