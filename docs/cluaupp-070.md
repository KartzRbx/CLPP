# Cluaupp 0.7.0 handoff

Pin **CL++ 0.7.0** in [KartzRbx/Cluaupp](https://github.com/KartzRbx/Cluaupp):

1. README / templates / tests: require `clpp --version` **0.7.0**. Refuse older compilers.
2. Optional npm packages `@cluaupp/clpp-win32-x64`, `@cluaupp/clpp-linux-x64`, `@cluaupp/clpp-darwin-arm64` (and x64) wrapping GitHub Release binaries.
3. `cluaupp watch` must not use `spawnSync` for every keystroke — spawn `clpp lsp` or reuse a long-lived `clpp api` process.
4. Consume `sourceMap` from `clpp api compile` / `*.luau.map` to map Luau diagnostics back to `.clpp`.
5. Language id `clpp` also covers `.flare .mint .bloom .helm .shift .hive .axiom`.
6. Cross-check `stdlib/libs.json` against Janitor (`Add`/`Cleanup`, not Sweep) and DataService (`WaitFor`/`Keep` mapped if the runtime differs).
7. Mason / editor installers should consume GitHub Release assets listed in `editors/mason/registry.json` (`v0.7.0`).

