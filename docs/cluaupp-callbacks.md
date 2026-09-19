---
title: Cluaupp — anonymous callbacks
---

# Cluaupp — anonymous callbacks

This note is for [Cluaupp](https://github.com/KartzRbx/Cluaupp): templates, snippets, generated `.clpp`, and tests that **write CL++ source**. The compile JSON contract (`clpp api compile`) did not change. The **language** did.

CL++ anonymous callbacks are now **`func (params) { }`**. The C++ capture list is gone.

## Write this

```clpp
players.PlayerAdded~>Connect(func (Player* playerEntered) {
    post("hello, " .: playerEntered.Name);
});

pcall(func () {
    return 1;
});

func onCoinsChanged = func (int newValue) {
    post(newValue);
};
```

Zero parameters still need the keyword and empty parens: `func () { }`.

## Do not emit this

These **do not compile**:

```clpp
func [](Player* player) { }
[]() { }
func []() { }
```

`[]` is **only** indexing (`names[0]`, `stats["Coins"]`, `folder[childName]`). It is not a lambda.

## Emit (unchanged shape)

Cluaupp still receives ordinary Luau from `clpp api compile`. A janitor Connect stays a janitor Connect:

```clpp
players.PlayerAdded~>Connect(func (Player* playerEntered) {
    post(playerEntered.Name);
});
```

```luau
janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	print(playerEntered.Name)
end), "Disconnect")
```

Manual connect (no Janitor) is `::Connect` with the same `func (` body.

## Grammar Cluaupp can rely on

The compiler requires:

```
func ( [params] ) { statements }
```

- Keyword `func` (also the type).
- `(` … `)` — parameter list may be empty.
- `{` … `}` — body. No `->`.

Named functions are unchanged: `void Greet(Player* player) { }`.

## Operators in generated source

When Cluaupp prints accessors next to a callback, use the current table:

| CL++ | Meaning |
| --- | --- |
| `.` | Property and instance method (`player.Name`, `player.Kick()`, `workspace.FindFirstChild("x")`) |
| `:` | Type on a name, or a protected (`pcall`) call (`age: int`, `player:Kick()`) |
| `::` | Static / class method definition / **manual** `Connect` |
| `~>` | `Connect` / `Once` with Janitor |
| `[]` | Index only |

## Diagnostics

`clpp api compile` returns `ok: false` plus `diagnostics[]` when a file still has `func []` / `[]()`. The editor pack lints the same line:

`use func (params) { } — CL++ does not use captures []`

After shipping a new `clpp`, Cluaupp users need a language-pack refresh so Error Lens / CL++ Lens shows that message: `clpp setup` / `clpp install`, or copy `editors/vscode` into the editor extensions folder, then reload.

## What to update in Cluaupp

1. **Scaffold / `init` templates** — every `Connect`, `Once`, `OnChange`, `pcall`, Vide/Fusion callback.
2. **Snippet generators** — stop concatenating `func [](` or `[](`.
3. **Golden `.clpp` fixtures** — rewrite to `func (Type name) { }`.
4. **Docs and UI copy** that showed `[]()` as “CL++ lambda”.
5. **Tests** that asserted `func []` still compiled — they should now expect failure.

The `CompileRequest` / `CompileArtifact` types in [`support/cluaupp.d.ts`](https://github.com/KartzRbx/CLPP/blob/main/support/cluaupp.d.ts) stay the same.

## Language pages

[func (...)](reference/lambda) · [func](reference/func) · [Connect](reference/Connect) · [operators](operators) · [JSON contract](cluaupp-support)
