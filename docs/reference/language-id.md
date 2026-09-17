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

**This site (Moonwave / Docusaurus).** Prism does not ship `clpp`. The docs build injects `src/theme/prism-include-languages.js`, loads Prism's C++ grammar, and aliases `clpp`. The API tab uses Refractor via `@mapbox/rehype-prism`; the same build aliases `cpp → clpp`. Without that registration, SSG throws `Unknown language: clpp is not registered`.

**GitHub.com.** [Linguist](https://github.com/github-linguist/linguist) highlights files from extensions. `.gitattributes` maps `*.clpp` to C++ so repository blobs have syntax color. Markdown fences on github.com stay plain until Linguist accepts a custom language. That is a separate registry from this site.

**How to add a new highlighter.** (1) Editors: `editors/vscode/syntaxes/clpp.tmLanguage.json`. (2) Docs site: `.moonwave/src/theme/prism-include-languages.js` plus `scripts/moonwave-docs.cjs`. (3) GitHub: a Linguist PR, not something this repo can finish alone.

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
