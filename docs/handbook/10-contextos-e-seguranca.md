---
title: Contexts and security
description: Server, client, and parallel policies for an example transpiler written in CL++, with explicit security limits.
---

# Contexts and security

A tool written in CL++ can analyze source intended for a client/server application. A Roblox/Luau transpiler is one such application; its policies belong to that tool, rather than to the CL++ runtime.

> **Official execution model:** CL++ runs on its own VM. There is no planned official Luau backend or official Roblox integration. The attributes, file-context rules, and engine diagnostics below describe an example transpiler that a developer could implement in CL++. They are not guarantees of the current compiler.

## Server authority

A client presents the interface and sends requests. A server must validate requests before changing authoritative state: identity, permissions, values, limits, and action order. A source attribute does not replace these checks.

An example transpiler could infer context from filenames or project configuration:

| Example source file | Application policy |
|---|---|
| `Server.server.clp` | Server code |
| `Interface.client.clp` | Client code |
| `Types.clp` | Shared module |

These filenames illustrate the tool's input convention. The CL++ VM does not automatically create Roblox execution contexts from them.

## `@server` and `@client`

```clp target
@server
void SavePoints(Player player, number points);

@client
void ShowScore(number points);
```

Read the attributes as context restrictions in the example tool's input language. Its checker would need to reject a direct call across incompatible contexts.

```clp target
<< Interface.client.clp: a call the example policy would reject.
void ShowResult(Player player) {
    SavePoints(player, 10);
}
```

`CLUAU_AUTH001` was suggested as an identity for this diagnostic. It is **proposed for the example tool**, not a verified diagnostic emitted by the CL++ compiler. A useful message would identify the call, its origin context, and the required context.

### Where the policy helps

- Server operations: granting items, validating purchases, and persisting data.
- Client operations: interface, input, and local effects.
- Shared modules: types and calculations independent of one authority.

A client action requiring server authority needs host communication and server-side validation. Marking a function neither creates that communication nor prevents exploitation of poorly validated endpoints.

## `@parallel`

The example tool could restrict effects in functions intended to run concurrently. A calculation that receives inputs and returns a result is easier to analyze than one that modifies shared state.

```clp target
def Vec2 { float X; float Y; }

@parallel
float ManhattanDistance(Vec2 a, Vec2 b) {
    return (a.X > b.X ? a.X - b.X : b.X - a.X)
         + (a.Y > b.Y ? a.Y - b.Y : b.Y - a.Y);
}
```

```clp target
number coins = 0;

@parallel
void Tick() {
    coins = 1; << shared-state mutation the example policy would reject
}
```

`CLUAU_PAR001` is a **proposed tool diagnostic**. Implementing the policy requires analysis of aliases, indirect calls, and transitive effects. A function without visible assignments can still call a mutating function. Reading shared data is not automatically safe while another task changes it.

## Policy summary

| Attribute in the example input | Intended restriction | Required analysis |
|---|---|---|
| `@server` | Calls in server context | Context inference, imports, and indirect calls |
| `@client` | Calls in client context | The corresponding checks in the other direction |
| `@parallel` | Effects compatible with concurrent execution | Shared state, aliases, and transitive effects |

The supplied material also mentioned `[[server]]` and `[[client]]`. Their equivalence is unspecified. A developer implementing the example tool must choose and document its accepted input grammar; these forms are not advertised as core compiler features.

## Organize source modules

```text
Types.clp                  shared definitions
Rules.clp                  calculations and rules
Server.server.clp          example authority and persistence input
Interface.client.clp       example presentation and input source
```

All are source files: **CL++ uses no headers**. See [Modules](09-modulos.md), [Events and concurrency](11-eventos-e-concorrencia.md), and [Transpilers and engine use cases](13-roblox-e-cluaupp.md).
