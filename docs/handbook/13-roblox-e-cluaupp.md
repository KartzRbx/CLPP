---
title: Transpilers and engine use cases
description: Build a transpiler in CL++ while keeping its target language and engine contracts separate from the official VM runtime.
---

# Transpilers and engine use cases

## A transpiler is a CL++ application

CL++ officially runs on its **own VM**. A developer can use CL++ to implement a transpiler: a program that reads another program, analyzes it, and writes source for a chosen target.

Roblox/Luau illustrates this use case. It is **not** a planned official backend, an official engine integration, or the official execution model of CL++.

| Component | Responsibility |
|---|---|
| Official CL++ compiler and VM | Compile CL++ to verified bytecode and execute it |
| Transpiler written in CL++ | Read its chosen input syntax, analyze it, and produce target source |
| Example Cluaupp application | Illustrative name from the supplied material for engine declarations, project policy, and translation tooling; no shipped package is established here |
| Target engine | Execute the generated application using its own runtime and APIs |

The transpiler's input grammar may resemble CL++. It must still define what it accepts; writing the tool in CL++ does not automatically add its grammar, attributes, or engine types to the CL++ compiler.

## Source modules, without headers

The tool itself uses CL++ source modules. Its engine declarations and project metadata can also be represented as source or data, according to its own design. **CL++ has no headers.** These filenames are an example organization, not an available package:

| Example module | Purpose |
|---|---|
| `Roblox.clp` | Service and engine-type declarations |
| `Instances.clp` | Engine-object models |
| `Datatypes.clp` | Engine-specific value models |
| `Janitor.clp` | Connection and resource cleanup helpers |
| `DataService.clp` | Persistence interface models |

```clp target
link "./Roblox.clp" as Roblox;
```

The path depends on files the developer actually implements. Importing a module does not create engine objects or install services in the VM.

## Example engine contracts

The following snippets show possible **input to the example transpiler**. They do not claim current VM APIs or an official CL++ engine binding.

### Objects and members

```clp target
void ShowName(Player player) {
    string name = player.Name;
    post(name);
}
```

The tool must define `Player`, its members, and how member access translates. A VM type and an engine type named `Vector3` need not have the same representation or behavior. The application must define interoperability explicitly.

### Services

```clp target
Players players = GetService<Players>();
RunService run = GetService<RunService>();
auto workspace = GetService<Workspace>();
```

`GetService<T>()` is an example typed service interface for the transpiler's input. The developer must implement its validation and translation. It is not a builtin installed by this handbook.

### Object creation

```clp target
auto part = Part {};
part.Anchored = true;
```

`Part {}` requires the application to supply a Part definition with suitable defaults. It uses the same named-initialization form as other CL++ data; engine allocation belongs to the adapter. See [Definitions and types](07-def-e-tipos.md).

### Engine events

```clp target
void ConnectPlayers() {
    Players players = GetService<Players>();
    players.PlayerAdded~>Connect(func (Player player) -> void {
        post("joined " .: player.Name);
    });
}
```

The transpiler application must define how the event maps to the target engine and who owns connection cleanup. `~>` alone is not a proven automatic-disconnection guarantee. See [Events and concurrency](11-eventos-e-concorrencia.md).

## Text output in the language

| Function | Purpose |
|---|---|
| `post(value)` | Ordinary information |
| `warn(value)` | Warning output |
| `report(value)` | Error output |

```clp target
number points = 10;
post("points: " .: points);
warn("low health");
```

`.:` concatenates values for display, including numbers; explicit string conversion is unnecessary here. In the official runtime, output goes through the process channels. An application translating source to another engine must implement any corresponding engine-output behavior itself.

## Context and effect policies

The supplied `@server`, `@client`, and `@parallel` examples can guide policies implemented by the transpiler application. Their checking requires context inference, host declarations, and effect analysis; they are not promises of the official compiler. The alternate `[[server]]` and `[[client]]` forms are unspecified. See [Contexts and security](10-contextos-e-seguranca.md).

## Implement the use case

1. Write the transpiler application in CL++ source modules.
2. Define its input grammar and engine declarations.
3. Implement parsing, name resolution, type/context checks, and source generation.
4. Run the transpiler application on the CL++ VM to generate its target files.
5. Send those files through the target project's chosen deployment or synchronization workflow.
6. Test the generated program in its target engine, including server-side validation of client requests.

These are tasks for the application developer. No `clpp build`/`init` command, automatic synchronization, or official Luau emitter is established by the example.

## Where a question belongs

- CL++ syntax, types, modules, and VM execution: the language documentation.
- Accepted transpiler input, generated source, and engine policies: the transpiler project's contract.
- Engine properties, events, and services: the engine's documentation.
- Deployment and synchronization: the target application's chosen tools.

Continue with [Modules](09-modulos.md) and [Errors and tools](12-erros-e-ferramentas.md).
