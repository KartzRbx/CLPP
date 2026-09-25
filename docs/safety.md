---
title: Context and parallel safety
description: CLUAU_AUTH and CLUAU_PAR run in clpp_ty and show up in the editor while you type.
---

# Context and parallel safety

The host tells the compiler whether the file is Client, Server, or Module. `*.client.clpp` is Client. `*.server.clpp` is Server. Every other file is Module. `clpp_ty` then checks `@client`, `@server`, and `@parallel`.

If either rule fails, `clpp_codegen` does not emit Luau.

## CLUAU_AUTH001

A Client file cannot declare or call an `@server` function. A Server file cannot declare or call an `@client` function.

```clpp
@server
void Save();

void init() {
    Save();
}
```

In `Boot.client.clpp` the editor marks `Save()`:

```text
CLUAU_AUTH001: call to server function `Save` from a Client context
```

The same source in `Boot.server.clpp` is clean.

```clpp
@client
void OpenHud();

void init() {
    OpenHud();
}
```

In a `.server.clpp` file that call is the same code, the other way: client-only authority on the server.

## CLUAU_PAR001

An assignment inside an `@parallel` region is a shared-memory write. The checker rejects it.

```clpp
@parallel
void tick() {
    coins = 1;
}
```

```text
CLUAU_PAR001: mutation is not allowed under @parallel
```

Reading and calling without `=` stays legal. The underline uses the byte span of that line, so the LSP range is the line you wrote, not a later Luau line.
