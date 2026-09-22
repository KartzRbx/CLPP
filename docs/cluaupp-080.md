# Cluaupp 0.8.0 handoff

Pin **CL++ 0.8.0** in [KartzRbx/Cluaupp](https://github.com/KartzRbx/Cluaupp):

1. README / templates / tests: require `clpp --version` **0.8.0**. Refuse older compilers.
2. Prefer `import { Name } from "./path"` for app modules; treat `#include` as legacy splice / angle includes only (RFC 0003).
3. Consume `nativeHints` / `layoutHints` from `CompileArtifact` (`support/cluaupp.d.ts`, `support/cluaupp-bridge.ts`) for selective `@native` and SoA/buffer layout.
4. Keep long-lived `clpp lsp` / `clpp api` processes for watch (no `spawnSync` per keystroke).
5. Consume `sourceMap` from `clpp api compile` / `*.luau.map` to map Luau diagnostics back to `.clpp`.
6. Mason / editor installers should consume GitHub Release assets listed in `editors/mason/registry.json` (`v0.8.0`).

See also [CHANGELOG](../CHANGELOG.md) and [CHANGELOG-0.8-DETAILED](CHANGELOG-0.8-DETAILED.md).
