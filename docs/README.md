---
unlisted: true
---

# docs/

The [CL++ course](intro.md) is the **user guide**. Every construct has a page under [reference](reference.md). Formal grammar and emit live in [spec/](spec/compiler.md).

The published site is Starlight (`npm run docs` → `www/`). Markdown in this folder is the source; `scripts/prepare-starlight.mjs` copies it into the Starlight collection so URLs stay `/docs/...`.
