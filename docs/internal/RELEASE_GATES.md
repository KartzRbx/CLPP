# CL++ VM 0.11.0 release gates

Updated 2026-10-16. This is the current inventory; historical runs are retained
in `.qa/REPORT.md` and `.qa/history/runs.jsonl`. The author authorizes completing
validation, installation, documentation, integration and GitHub release delivery.
The independent Luau product and unrelated user changes must remain intact.

## Implemented current language contract

- [x] Current lexer/parser forms, comments `<<` / `<<[ ... ]>>`, `@this`, explicit
  function arguments, checked `(Type expression)` and rejection of legacy forms.
- [x] Exact signed 64-bit `number`, decimal `float`, checked arithmetic/casts,
  literal/union/optional types, nominal data and invariant mutable collections.
- [x] Generic bounds, specialization, callbacks, closures and reference captures;
  one storage base plus fieldless contracts; bound and async method dispatch.
- [x] Managed event caretaker, ordered error-stopping synchronous handlers,
  observable connections, cooperative waits/tasks and execution resource cleanup.
- [x] Independent modules/private state, shared identity, deep const, guards,
  contextual array construction, JSON values and generic CLI file/process APIs.
- [x] Imported runtime origin through module nesting, workers and CLIR revision 4;
  expected/received type details and thread-isolated emission source positions.
- [x] Regression protection for parser budgets, bounded transport/CLIR input,
  concurrent compilation, diagnostic lifetime, host ranges and worker completion.

These boxes identify implemented contracts with regression coverage, not a
mathematical proof that all possible programs are sound or every defect is absent.

## Final delivery evidence

- [x] Normal Windows GCC suite: 544 checks passed before the additional editor
  discovery regression; that regression separately passed its three scenarios.
- [x] Debug/Release comparison: 192 independent Python-oracle scenarios passed.
- [x] Windows installer: installation/update, bad-checksum rejection, ownership
  checks and preservation of unrelated files have executable regressions.
- [x] Prior integrated matrix passed Linux, macOS, MSVC, Windows/Linux sanitizers,
  portable Windows package and composed documentation. This is baseline evidence.
- [x] Final local ASan/UBSan: 543/543; 1,552 normal adversarial cases passed.
  Additional instrumented mutation results are recorded in the QA report.
- [x] Five output-verified benchmarks and composed site/link checks completed.
- [ ] Run this exact candidate on all remote platforms and inspect every result.
- [ ] Validate final ZIP/VSIX identities, isolated installation and actual public
  download checksums; install matching local compiler/editor artifacts.
- [ ] Merge reviewed integration, build/verify the composed site and confirm Pages.
- [ ] Publish 0.11.0 from successful CI artifacts for the exact main revision.

## Deliberately outside this release contract

The author deferred protected colon calls, destructuring and optional missing-key
lookup. The 19 target-marked documentation blocks also include native host effects,
conversion and application/transpiler policies. They remain visible and excluded;
none is counted as executed or implemented merely to close a release checkbox.

Streaming/cancellable process handles, native filesystem watching/content hashing,
source-level compiler APIs, standalone source-program executable generation,
general FFI and user-defined/block destructors remain separate future work.
Broader UI components and physical audio/input/device coverage remain native
package work. Visual editor/profile screenshots and Windows leak detection are
separate from the automated correctness results. No general speed advantage or
zero-vulnerability guarantee is claimed.
