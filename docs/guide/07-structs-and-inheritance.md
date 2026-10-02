# 7. Structs and inheritance

A `struct` groups typed fields and methods. Construct a value by calling its type with field values. Methods receive an implicit `self`; `@field` is shorthand for `self.field` inside a method.

```clp
struct Player {
  string name;
  int hp;

  func damage(int amount) {
    @hp = @hp - amount;
  }
}

Player hero = Player("Ada", 100);
hero.damage(30);
post(hero.hp);
```

Struct values copy on assignment. A method that mutates `self` changes its receiver. A struct can inherit from one or more base structs with `struct Child : Base { ... }`. Inherited fields and methods remain available on the derived type.

Use `abstract` for a required method, `override` for a replacement in a derived struct, and `final` to prevent further overriding. `private` restricts a member to its declaring struct; `public` exposes it. Virtual dispatch selects the derived implementation when calling an overridden method through a base type.

Operator methods such as `func operator+` let a struct participate in expressions. Type parameters let a struct hold values of different types without giving up static checking.

## Choosing composition or inheritance

Use a struct when several values and operations belong to one concept, such as a fighter with HP and an attack method. A field can itself be another struct, which is often the simplest way to combine behavior. Inheritance fits a genuine "is a" relationship where callers need to treat different derived types through a shared base. Avoid a deep inheritance tree merely to reuse a few fields; composing smaller structs keeps initialization easier to see.

Use `abstract` when a base promises behavior that every concrete child must implement. Add `override` to make an intentional replacement explicit, and `final` when further replacement would violate a rule. Keep implementation details `private` when callers should use methods instead of modifying fields directly.
