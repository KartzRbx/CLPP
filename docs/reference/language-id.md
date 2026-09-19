---
title: "Language id clpp"
sidebar_label: "clpp fence"
---

# Language id clpp

<div class="clpp-ref-meta">Tooling</div>

How CL++ is registered as a language — editors, this documentation site, and GitHub.

## Syntax

```clpp
post("hello");
```

## Parameters

None.

## Return value

N/A

## Luau emit

`N/A`

## Description

CL++ is **not** C++. Fences must use the language id `clpp` (aliases `clp`, `clh`).

**Editors.** `clpp install` copies the VS Code / Cursor pack. `package.json` contributes language id `clpp` for `*.clpp` / `*.clp` / `*.clh`, with grammar `source.clpp`. That is a TextMate registry, not Prism.

**This site (Starlight / Astro).** Fences use the language id `clpp` (aliases `clp`, `clh`). The highlighter is the same TextMate grammar as the editor pack (`editors/vscode/syntaxes/clpp.tmLanguage.json`), loaded into Shiki. `@` is punctuation, `this` is the language receiver, `@field` is a property.

**GitHub.com.** [Linguist](https://github.com/github-linguist/linguist) highlights files from extensions. `.gitattributes` maps `*.clpp` to C++ so repository blobs have syntax color. Markdown fences on github.com stay plain until Linguist accepts a custom language. That is a separate registry from this site.

**How to add a new highlighter.** (1) Editors: `editors/vscode/syntaxes/clpp.tmLanguage.json` — keep `#receiver` so `@` and `@this` are `keyword.other.receiver.clpp`, and `@field` keeps that keyword on `@`. (2) Docs site: that same grammar is registered in `www/astro.config.mjs`. (3) GitHub: a Linguist PR, not something this repo can finish alone.

## Notes

Do not use the fence language `cpp` for CL++ samples on this site. The fence id is part of the language identity.

## Example

```clpp
post("hello");
```

Emits:

```luau
print("hello")
```

## See also

[Install](../install) · [post](post)
