# 10. Modules

Each `.clp` file is a module. `link` imports declarations from another file or a built-in `@clpp.*` module. An alias gives the imported names a prefix. Imported files can contain declarations and literal constants, but no top-level executable statements.

```clp fragment
link @clpp.axiom as Axiom;
link "./player.clp" as PlayerModule;

post(Axiom.Clamp(150, 0, 100));
```

Without `as`, the final component of the module name becomes its alias with an initial capital letter. `using Alias.member;` brings a member into the current scope. `import` is also accepted as an import declaration.

File paths in `link` are resolved from the directory of the entry file passed to `clpp`, including links inside imported modules. For example, when running `clpp src/main.clp`, a module at `src/core/game.clp` imports `src/entities/ghost.clp` with `link "./entities/ghost.clp" as Ghost;`. Keep all project links relative to `src` in this setup. Running `clpp src/core/game.clp` directly gives that file a different entry directory, so its project links must be adjusted or the program should be started through `src/main.clp`.

The built-in modules are `@clpp.axiom` (game math), `@clpp.text` (strings), `@clpp.math`, `@clpp.fs` (files), `@clpp.os` (environment), `@clpp.http` (URLs), `@clpp.window` (window and input), `@clpp.gfx` (2D graphics), `@clpp.ui` and `@clpp.gui` (interfaces), `@clpp.audio`, `@clpp.io`, `@clpp.input` (automation), `@clpp.json`, and `@clpp.time`. Their APIs are covered in the [standard library](14-standard-library.md) and the final three guide chapters.

## Design a module boundary

Group declarations that change for the same reason. A combat rules module can export `damageFor` and `hit`, while a UI module renders the result. Keep top-level orchestration in the entry file and keep imports free of side effects. Use an alias when several modules export similarly named members. Import cycles are reported as errors, so move shared types to a lower-level model module instead of making two modules import each other. See the [modular combat project](20-modular-combat.md) for a complete directory layout.
