# scripts/

Build, test, CI, and repository utility scripts.

| Script | Role |
| --- | --- |
| `moonwave-docs.cjs` | Runs Moonwave and registers Prism/Refractor language id `clpp` so markdown fences ` ```clpp ` highlight. Used by `npm run docs` / `docs:build`. |
| `write-reference.mjs` | Regenerates `docs/reference.md` and `docs/reference/*.md` (one page per language utility). |
