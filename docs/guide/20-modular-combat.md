# 20. Project: a modular combat system

This tutorial builds a small turn of combat from separate source files. It illustrates which responsibilities belong in a data model, a rules module, a presentation module, and the entry point. Copy the files into one directory and run `clpp main.clp` there.

```text
combat-demo/
├── model.clp    Fighter data
├── rules.clp    Damage and healing rules
├── hud.clp      Text presentation
└── main.clp     Program entry point
```

## 1. Model the state

`Fighter` has only data and a small query method. Other modules can construct it through the `Model` alias. An imported module contains declarations and literal constants; it does not execute a top-level program.

```clp file=model.clp
struct Fighter {
  string name;
  int hp;
  int attack;

  func isAlive() -> bool { return @hp > 0; }
}
```

Use a struct here because each fighter needs its own typed state. Value semantics make it easy to compute an updated fighter without unexpectedly changing a copy held elsewhere.

## 2. Put rules in their own module

The rules module imports the model and returns an updated fighter. `Axiom.Clamp` keeps HP within the allowed range. `damageFor` keeps the damage formula in one place, where changing the balance does not require editing the UI.

```clp file=rules.clp
link "./model.clp" as Model;
link @clpp.axiom as Axiom;

const MAX_HP = 100;

func damageFor(int attack, int defense) -> int {
  return Axiom.Clamp(attack - defense, 0, attack);
}

func hit(Model.Fighter target, int damage) -> Model.Fighter {
  target.hp = Axiom.Clamp(target.hp - damage, 0, MAX_HP);
  return target;
}

func heal(Model.Fighter target, int amount) -> Model.Fighter {
  target.hp = Axiom.Clamp(target.hp + amount, 0, MAX_HP);
  return target;
}
```

Returning a new value is useful when rules should be easy to test and replay. For an object owned by one part of the program, a mutating method can be simpler.

## 3. Keep output separate

The HUD knows how to display a fighter, but knows nothing about damage calculations. A graphical game could replace this module with one using `@clpp.gfx` and `@clpp.gui` without changing the rules.

```clp file=hud.clp
func show(string name, int hp) {
  post(name .: ": " .: hp .: " HP");
}
```

## 4. Connect modules in the entry point

Only `main.clp` has top-level actions. It imports each module, creates fighters, applies the rules, and displays the outcome.

```clp
link "./model.clp" as Model;
link "./rules.clp" as Rules;
link "./hud.clp" as Hud;

Model.Fighter hero = Model.Fighter("Ada", 100, 18);
Model.Fighter enemy = Model.Fighter("Golem", 40, 12);

let damage = Rules.damageFor(hero.attack, 3);
enemy = Rules.hit(enemy, damage);
Hud.show(enemy.name, enemy.hp);

hero = Rules.hit(hero, Rules.damageFor(enemy.attack, 5));
hero = Rules.heal(hero, 2);
Hud.show(hero.name, hero.hp);
post(enemy.isAlive());
```

```output
Golem: 25 HP
Ada: 95 HP
true
```

## Why this structure helps

The model defines what exists, rules define what can happen, the HUD controls presentation, and the entry point chooses the sequence. Imports make dependencies visible. Keep modules small enough to understand, but avoid a separate module for every tiny function: split a module when it has a distinct responsibility or several callers.

To extend the system, add an `enum` for damage type, a `signal` for combat events, an `array<Model.Fighter>` for a party, or JSON save data. [Structs](07-structs-and-inheritance.md), [events](13-events.md), and [JSON](19-audio-console-automation.md) explain those pieces.
