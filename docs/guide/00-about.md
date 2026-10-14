# About CL++

CL++ is a language for games and tools. Its author wants readable typed code, data definitions, composable game logic, and useful compiler feedback. A beginner should be able to read the syntax before learning any library.

## What kind of language is it?

CL++ is a general-purpose, high-level, multiparadigm language with static typing, inference, and its own VM. It has an imperative and procedural foundation, with object, event and functional facilities in the language design. Event-driven programming is an option for applications, not the only execution style. CL++ is not a markup language; declaring a UI through a library is still programming in CL++.

The [handbook introduction](../handbook/00-o-que-e-o-clpp.md) explains this classification and separates current compiler support from pending features.

## Language first

The target language uses typed variables, `const` for fixed values, `def` for types, functions with their return type before the name, typed loops, and `.clp` modules without headers. The author's handwritten Combat program is the reference for this writing style.

Start with [Learn CL++ from zero](00-language-tour.md), then follow the separate lessons for declarations, operators, flow, functions, types, and modules. The [syntax map](../reference/syntax.md) and [lexer reference](../reference/lexer.md) explain the writing rules directly.

## What you can build

- Game logic: movement, combat, abilities, state machines, and events.
- Playable prototypes: windows, drawing, input, UI, and sound.
- Desktop tools: forms, files, JSON, and console operations.
- Automation and embedded applications where host facilities are available.

The [standard library](14-standard-library.md) explains the existing APIs separately from language syntax. Complete project chapters show how the intended language organizes application code.

## A compiler update is pending

The current repository uses C++20, bytecode, and a VM, but portions of its public syntax still reflect an older design. This site documents the author's target language first. Target specification blocks do not count as passing executable tests; they do not claim the complete example already runs. See [Implementation status](../reference/status.md).

The VM's existing historical benchmarks are described in [Performance](16-performance.md). They do not measure a compiler implementation of all the target syntax.
