# What is CL++?

**CL++ brings typed code, clear contracts, and useful tools together for games and applications. Its official execution model is its own virtual machine.**

## Language classification

CL++ is a general-purpose, high-level, multiparadigm language with static typing, type inference, and execution on its own VM. Its foundation is imperative and procedural: functions describe operations, conditions select branches, and loops repeat work.

The language design also includes objects defined with `def`, signals and callbacks for event-driven applications, and functional facilities such as function values and closures. Events are one way to organize an application; they do not define every CL++ program. **CL++ is not a markup language.** Declarative UI APIs are libraries used from CL++ code.

Typed declarations, functions, data definitions, callbacks, reference-captured closures, shared object identity and typed signals are VM-tested. See [Implementation status](../reference/status.md) for the remaining language and native-library work.

The repository's C++20 compiler checks source, generates verified bytecode, and executes it on the CL++ VM. This handbook teaches the author's target syntax; a target example does not prove support in an installed release. There is no official Luau backend or promised Roblox integration.

Roblox is one possible project domain: you could write a transpiler **in CL++** that generates readable Luau, then connect its output to your own tooling. That application would define its host APIs, emission rules, and lifecycle. An application can expose a player-added notification through a typed signal. This complete VM example demonstrates registration and managed cleanup:

```clp
def Player { string Name; }
def Players { signal<Player> PlayerAdded = signal<Player>(); }
void ConnectPlayers(Players players) {
    players.PlayerAdded~>Connect(func (Player player) -> void {
        post("hello, " .: player.Name);
    });
}
auto players = Players {};
ConnectPlayers(players);
players.PlayerAdded::Fire(Player { Name = "Ada" });
players.PlayerAdded::Clear();
players.PlayerAdded::Fire(Player { Name = "Lin" });
```

```output
hello, Ada
```

An external engine adapter must supply the actual source of player notifications. The example verifies the VM signal contract and the application's data types.

## Status labels: read these first

This handbook describes the language **as its author intends it to be**. Important sections distinguish design from implementation:

| Label | Meaning |
| --- | --- |
| **Target specification** | Source spelling taught here; implementation and tests must establish compiler support |
| **Proposed** | A design requirement, including types, callbacks, formatting, and visibility, awaiting implementation |
| **Verify** | A contract or detail not yet validated against the repository's lexer, parser, checker, and tests |

A page can explain a design before implementation when that difference is explicit. A `target` block never counts as a passing executable test. Check [compiler status](../reference/status.md) before using a release.

## The problem CL++ addresses

As a project grows, implicit assumptions become difficult to maintain:

- A misspelled field can survive in code with insufficient type information.
- An authority rule can remain implicit and be discovered too late without suitable tools.
- Two team members can disagree about what a function accepts.
- An unexpected absent value can cross several functions before an error appears.

The checker aims to catch these mistakes **before the program runs**. Other languages, including Luau, also provide types and static analysis. The intended gain is CL++'s own contracts and checks, not a claim that other languages detect everything only at runtime. This is a familiar tradeoff: make contracts explicit now to reduce surprises later.

## Three ideas to remember

**1. Declared.** Write `number health = 100;` so readers can see what the value means. Use inference when the initializer is clear. A type communicates intent without executing the program.

**2. Checked.** The intended checker should detect missing `match` cases and unsafe use of values that may be `null`. A host application or user-written transpiler can add domain rules, such as client/server authority; those rules are not an official Roblox integration.

**3. Understandable.** Source, diagnostics, module boundaries, and examples should make behavior clear. If your own project emits another language, readable generated output is a useful requirement of that project, rather than a different official CL++ execution model.

## Who it is for

- Developers coming from **C++, C#, Java, or TypeScript** who want familiar typed writing.
- **Teams** that need clear contracts between source files.
- **Medium and large projects** where late errors cost time.
- Beginners who want to **learn types** through focused examples.

## When to use it

Consider CL++ when several modules cooperate, shared rules need explicit contracts, editor feedback matters, or multiple people maintain the same code. Its existing graphics, UI, audio, and file APIs support practical programs on the VM.

Another language may fit better when the program is a short disposable script, your team already has the tools and typing it needs, or you require C++ itself. CL++ borrows familiar writing without C++ pointers, full templates, or manual memory management. Read the [mental model](./02-modelo-mental.md).

## A one-minute tour

```clp
pragma strict;

def Player {
    string Name;
    number Health = 100;
}

void Heal(Player player, number amount) {
    player.Health += amount;
}

Player ana = Player { Name = "Ana" };
Heal(ana, 20);
post(ana.Name .: " has " .: ana.Health);
```

```output
Ana has 120
```

Read it aloud: define a player with a name and health initially set to one hundred; heal Ana by twenty; print her name and health. The target object model shares identity through the argument. The current compiler supports data definitions, typed functions and constants, and file pragmas; shared object identity and typed callbacks are compiler/VM-tested.

## How the handbook is organized

| Chapter | Question it answers |
| --- | --- |
| [1. Getting started](./01-comecando.md) | How do I install and run CL++? What is a `.clp` module? |
| [2. Mental model](./02-modelo-mental.md) | What does the compiler do, and what runs on the VM? |
| [3. Types and variables](./03-tipos-e-variaveis.md) | How do I store values with `auto`, `optional`, and `null`? |
| [4. Operators](./04-operadores.md) | What do `.`, `::`, `:`, `.:`, and `~>` mean? |
| [5. Control flow](./05-controle-de-fluxo.md) | How do `if`, loops, `match`, and `guard` work? |
| [6. Functions](./06-funcoes.md) | How do I declare, pass, and store functions? |
| [7. Definitions and types](./07-def-e-tipos.md) | How do I describe data, inheritance, and generics? |
| [8. Collections](./08-colecoes.md) | How do `array`, `vector`, and `dictionary` store values? |
| [9. Modules](./09-modulos.md) | How do I split a project and connect it with `link`? |
| [10. Contexts and safety](./10-contextos-e-seguranca.md) | Which context and checking rules belong to the design or a host application? |
| [11. Events and concurrency](./11-eventos-e-concorrencia.md) | How do signals, cleanup, asynchronous work, and result values fit together? |
| [12. Errors and tools](./12-erros-e-ferramentas.md) | How do I interpret diagnostics and migrate source? |
| [13. Roblox and Cluaupp](./13-roblox-e-cluaupp.md) | How could a user-written transpiler and external engine tooling be organized? |
| [14. Reference](./14-referencia.md) | Where are the glossary, quick reference, comparisons, and limits? |
