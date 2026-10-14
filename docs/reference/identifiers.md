# Identifiers and names

An identifier names a variable, type, function, field, or module alias. A declaration creates the name; an expression can then use it within its scope.

## Writing a name

Use a letter or `_` first, followed by letters, digits, or `_`. Prefer ASCII identifiers for predictable tool support; text values can contain Unicode. Names are case sensitive: `Hp`, `hp`, and `HP` are different names.

| Valid examples | Purpose |
| --- | --- |
| `hp`, `player2`, `_index` | Variables and parameters |
| `Fighter`, `DamageCtx` | Types |
| `ResolveDamage`, `IsAlive` | Functions |
| `Combat`, `Axiom` | Module aliases |
| `MAX_HP` | Named constants |

A name cannot begin with a digit, contain a space, or use a reserved word. Use the [keyword index](keywords.md) to check language words.

## Declaration and use

```clp
number hp = 100;
hp -= 25;
post(hp);
```

The first line creates `hp`; the next line changes it; the last line reads it. Writing an undeclared name is a name-resolution error, not a new declaration.

## Member names

```clp
def Fighter { number Hp = 100; }
Fighter hero = Fighter {};
post(hero.Hp);
```

```output
100
```

`hero` identifies the value. `Hp` identifies a member of its type. A dot connects the receiver and the member. Enum cases use scope access such as `Element::Fire`; module imports use an alias such as `Combat.ResolveDamage`.

## Local scope

```clp
number hp = 100;
if (hp > 0) {
    number damage = 25;
    post(damage);
}
post(hp);
```

`damage` belongs to the block where it is declared; `hp` belongs to the enclosing scope. Give a value a new name when that makes its role clearer. See [Variables](../guide/03-variables-and-types.md) and [Modules](../guide/10-modules.md) for visibility rules.
