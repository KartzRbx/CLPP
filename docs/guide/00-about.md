# About CL++

CL++ is a language for games and tools. Its author wants readable typed code, data definitions, composable game logic, and useful compiler feedback. A beginner should be able to read the syntax before learning any library.

## What kind of language is it?

CL++ is a general-purpose, high-level, multiparadigm language with static typing, inference, and its own VM. It has an imperative and procedural foundation, with object, event and functional facilities in the language design. Event-driven programming is an option for applications, not the only execution style. CL++ is not a markup language; declaring a UI through a library is still programming in CL++.

The [handbook introduction](../handbook/00-o-que-e-o-clpp.md) explains this classification and separates current compiler support from pending features.

## Language first

The current language uses typed variables, `const` for fixed values, `def` for types, functions with their return type before the name, typed loops, and `.clp` modules without headers. The author's handwritten Combat program is the reference for this writing style.

Start with [Learn CL++ from zero](00-language-tour.md), then follow the separate lessons for declarations, operators, flow, functions, types, and modules. The [syntax map](../reference/syntax.md) and [lexer reference](../reference/lexer.md) explain the writing rules directly.

## What you can build

- Game logic: movement, combat, abilities, state machines, and events.
- Playable prototypes: windows, drawing, input, UI, and sound.
- Desktop tools: forms, files, JSON, and console operations.
- Automation and embedded applications where host facilities are available.

The [standard library](14-standard-library.md) explains the existing APIs separately from language syntax. Complete project chapters show how the intended language organizes application code.

## Current release and future proposals

CL++ VM 0.11.0 implements the current syntax and contracts documented by the
executable examples: typed declarations, generics, modules, reference captures,
events and checked type-first expressions. Removed syntax is rejected.
The [implementation status](../reference/status.md) separates these contracts from
deferred proposals, native-device coverage and application-specific host policy.
The 19 target specification blocks remain excluded from compiler tests.

[Performance](16-performance.md) explains the benchmark method and its limits.
Output checks and elapsed measurements describe the actual VM; they are not a
promise that every program is faster than another language.
