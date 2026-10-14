# 20. Project: a modular combat system

This tutorial builds a small turn of combat from separate source files. It illustrates which responsibilities belong in a data model, a rules module, a presentation module, and the entry point. The files below are executed together by the documentation checks. Public declarations use explicit export. Copy the files into one directory and run clpp main.clp there.

```text
combat-demo/
├── model.clp    Fighter data
├── rules.clp    Damage and healing rules
├── hud.clp      Text presentation
└── main.clp     Program entry point
```

## 1. Model the state

`Fighter` groups the data for one combatant. Other modules can construct it through the `Model` alias. Each module declares its own data, functions, and constants in a `.clp` source file. No headers are needed.

```clp file=model.clp
export def Fighter {
  string name;
  number hp;
  number attack;
}
```

Use a `def` here because each fighter needs its own typed state. The model groups the state under one named type. The intended `def` design has reference semantics: avoid assuming that assigning or passing a fighter makes an independent copy.

## 2. Put rules in their own module

The rules module imports the model, updates the fighter it receives, and returns that fighter. `Axiom.Clamp` keeps HP within the allowed range. `damageFor` keeps the damage formula in one place, where changing the balance does not require editing the UI.

```clp file=rules.clp
link "./model.clp" as Model;
link @clpp.axiom as Axiom;

const number MAX_HP = 100;

export number damageFor(number attack, number defense) {
  return Axiom.Clamp(attack - defense, 0, attack);
}

export Model.Fighter hit(Model.Fighter target, number damage) {
  target.hp = Axiom.Clamp(target.hp - damage, 0, MAX_HP);
  return target;
}

export Model.Fighter heal(Model.Fighter target, number amount) {
  target.hp = Axiom.Clamp(target.hp + amount, 0, MAX_HP);
  return target;
}
```

Returning the target keeps the update visible in the calling code. Under the intended reference semantics, the original fighter is also affected; create a separate fighter explicitly when you need independent state.

## 3. Keep output separate

The HUD knows how to display a fighter, but knows nothing about damage calculations. A graphical game could replace this module with one using `@clpp.gfx` and `@clpp.gui` without changing the rules.

```clp file=hud.clp
export void show(string name, number hp) {
  post(name .: ": " .: hp .: " HP");
}
```

## 4. Connect modules in the entry point

Only `main.clp` has top-level actions. It imports each module, creates fighters, applies the rules, and displays the outcome.

```clp
link "./model.clp" as Model;
link "./rules.clp" as Rules;
link "./hud.clp" as Hud;

Model.Fighter hero = Model.Fighter { name = "Ada", hp = 100, attack = 18 };
Model.Fighter enemy = Model.Fighter { name = "Golem", hp = 40, attack = 12 };

number damage = Rules.damageFor(hero.attack, 3);
enemy = Rules.hit(enemy, damage);
Hud.show(enemy.name, enemy.hp);

hero = Rules.hit(hero, Rules.damageFor(enemy.attack, 5));
hero = Rules.heal(hero, 2);
Hud.show(hero.name, hero.hp);
post(enemy.hp > 0);
```

```output
Golem: 25 HP
Ada: 95 HP
true
```

## Why this structure helps

The model defines what exists, rules define what can happen, the HUD controls presentation, and the entry point chooses the sequence. Imports make dependencies visible. Keep modules small enough to understand, but avoid a separate module for every tiny function: split a module when it has a distinct responsibility or several callers.

To extend the system, add an `enum class` for damage type, a `signal` for combat events, an `array<Model.Fighter>` for a party, or JSON save data. [Data definitions](07-structs-and-inheritance.md), [events](13-events.md), and [JSON](19-audio-console-automation.md) explain those pieces.
