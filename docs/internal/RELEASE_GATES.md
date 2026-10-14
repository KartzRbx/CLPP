# CL++ release gates

Updated: 2026-10-12. This is a live release checklist, not a claim that the language is complete. The newest section of RELEASE_HANDOFF.md holds the validation checkpoint; older sections are historical. Publishing is authorized directly, without a PR, **after the author-defined syntax is complete and these gates have evidence**. Do not replace unfinished features with a preview release or silently skip examples.

## Implemented foundations (retain regression coverage)

- [x] Own VM as official execution target; no official Luau backend.
- [x] Canonical number/float storage and checked core arithmetic/casts.
- [x] Distinct runtime Boolean kind through core operators, host predicates, event handles, JSON and bytecode serialization/verification; native bool inputs validated. 426 unit cases / 4,235 assertions and 478 CTest checks passed at this stage. Full native API audit remains separate.
- [x] Shared managed identity for data/collections; deep const through all aliases; identity equality.
- [x] Structured worker completion and execution-local shared resources.
- [x] Core synchronous typed callbacks and reference-captured local cells.
- [x] Explicit concrete generic function values, including checked constraints, imported types, returned properties and worker invocation.
- [x] Closures inherit enclosing generic formals/bounds and retain reference captures through nested scopes, modules and generic methods.
- [x] Explicit module exports, importer-relative paths, private state and once-per-run initialization.
- [x] Entry-file state visible to functions/callbacks with source-ordered initialization, precise types and lexical shadows.

These completed foundations do not imply complete native libraries, type-system corner cases, async scheduling, or distribution readiness.

## Language implementation

- [x] Contextual callback parameter inference from unambiguous expected signatures; checked explicit annotations, independent generic argument evidence, nested generic captures, modules, observables and IDE metadata.
- [x] Explicit function values specialized with enclosing generic formals, including constraints, modules, nested captures, owner/method scopes and concrete returned signatures.
- [x] Typed async function values and lambdas, preserved task<T> payloads and captures, spawn/await/parallel, error observation, actor restrictions and serialized executable metadata.
- [x] Bound receiver/method values and direct async virtual method scheduling, with specialized signatures, reference identity, virtual dispatch, access checks and serialized execution.
- [x] Typed signal subscriptions and ~>Connect with the author-confirmed internal event caretaker; Clear disconnects managed connections, ordered synchronous dispatch stops on first error, and callback retention/cleanup is checked.
- [x] Observable OnChange returns connection handles with ordered snapshot notifications, idempotent Disconnect and retained-callback release.
- [x] Author-approved native resource cleanup on execution completion, including errors and after worker drain: audio/device/mixer, window/timer/input, canvas/images/cache/fonts and interface state.
- [x] Wait/cooperative suspension: per-thread task queues, preserved frames/captures/traps, typed void/single/tuple payloads, one-shot subscriptions, cached results/errors, parallel dependencies, native producers, actor isolation, cross-worker observation, task mutex ownership and cancellation. Bytecode 11; 417 unit cases / 4,130 assertions and 469 CTest checks passed. Block-scope/user-defined destructors are not defined.
- [x] Generic definition-time field defaults retain the declaring owner scope and specialize per construction, including inherited collection/signal/callback defaults and module identities.
- [x] Typed array brace initializers use declaration/return/field/assignment/call context, including aliases, nested generic arrays, callbacks and imported nominal types; incompatible elements and missing type context are rejected. 419 unit cases / 4,158 assertions and 471 CTest checks passed; bytecode remains 11.
- [x] Author-confirmed removal of function-parameter defaults; configuration uses def field defaults and explicit arguments, including migrated GUI/audio/input APIs.
- [x] Implicit method receiver is @this only: internal unlexable receiver binding, no implicit self source name; migrated fixtures and rejected old reads/writes/returns/generic/async/closure/external methods. All 41 example sources audited recursively, 35 runnable scenarios have expected outputs, and example audit is registered in CTest. 427 unit cases / 4,245 assertions and 488 CTest checks passed.
- [x] Unique function names reject source overloads across arities, generics, async, methods and imports; Axiom/Gfx operations migrated to distinct public names with native ID positions preserved. 422 unit cases / 4,189 assertions and 474 CTest checks passed.
- [x] Named-call arguments removed; functions use positional arguments, while def construction retains named fields. Fixtures retain output checks. 423 unit cases / 4,202 assertions and 475 CTest checks passed.
- [x] Typed/auto constexpr declarations preserve exact numeric folding and literal types, reject bare legacy syntax, support exported constants and keep checked overflow/division by zero. 424 unit cases / 4,213 assertions and 476 CTest checks passed.
- [x] Preserve repeated/composed generic bounds and validate every intersection layout; compatible single-base intersection member views and formal proof are tested.
- [x] Canonical template/type-first functions accept where clauses, preserve template-header bounds and parse complete composed constraints; exported functions, malformed clauses and conflicting bounds are covered. Guide/EBNF/editor hover aligned.
- [x] External typed methods use local definition owners, generic receiver specialization and @this; ordinary access/type checks and virtual/bound/async dispatch preserved. 420 unit cases / 4,170 assertions and 472 CTest checks passed.
- [ ] Finish remaining type-system/effect cases and sound refinement around shared state and mutating calls. Abstract/async nominal interface support is now validated separately below.
- [x] One storage base plus fieldless abstract contracts: abstract signatures, generic diamonds/projection, per-member composed constraints, public contract compatibility, callbacks/async views and CLIR dispatch (bytecode 10). Remaining refinement/effect audits stay open.
- [x] Compiler, checker and bytecode-generator mutable context isolated per thread; concurrent compilation/IDE analysis and nested module-loader compilation tested.
- [ ] Move checker/binder state into explicit contexts where required; verify compiler/IDE reentrancy.
- [ ] Remove obsolete accepted grammar, public metadata, wrappers and fixtures without weakening regression coverage. No headers, let/mut, old struct/function/variant/option/result spellings or old numeric names as public language syntax.
- [ ] Audit every native boundary for exact number versus float, real boolean types, index/range validation and owned resource handles. Unknown results still using any must receive justified API types.
- [x] Typed recursive Json.Value via ParseValue/StringifyValue, exact signed integers, distinct booleans/decimal categories, Unicode-pair checks and cycle/payload/depth errors. Legacy Parse/Get remain explicit dynamic compatibility APIs.

## Executable specification and tooling

- [x] Execute migrated Combat deterministic battle and Pacman VM core with golden outputs; execute the modular Combat documentation project.
- [ ] Complete remaining author host/transpiler application contracts, effect annotations and expected diagnostic fixtures. The Roblox client/persistence references are not validated by the VM core tests.
- [ ] Implement and execute every target specification block. **73 target blocks remain excluded**. Current documentation batch: 155 executable examples / no failures. Checked cases cover output transcripts, expected const/nominal rejection, real exported module fixtures, enums, guards, aliases, generic objects, callbacks, collections and player-signal registration/Clear. README examples are included. Actual external engine adapters remain an open host gate. Compilation alone is separate from execution and assertion coverage.
- [ ] Audit site, README, reference, lexer grammar, manifest/generated diagnostics and editor syntax/snippets against the final compiler; public site remains English.
- [ ] Verify properties, variables, generic results, callback signatures, completion/hover/definitions/references and diagnostics with real VS Code installation. Retain the author's VS Code-only editor preference; do not reinstall Cursor. Passing integration tests alone do not mean an installed extension is updated.
- [ ] Run meaningful end-to-end projects, malformed-input/bytecode tests, concurrency/lifetime stress, sanitizers and every supported distribution platform. Record tools/platforms and failures explicitly; Windows debug checks are not a substitute for release/platform validation.

## Distribution and direct publication

- [ ] Final version, changelog and release build/artifact compatibility.
- [ ] Package executables/native libraries, checksums and VSIX; test from clean install and upgrade, commands/PATH/uninstall and editor detection.
- [ ] Confirm installer fetches the exact new release and does not install an older version.
- [ ] CI validation and Pages deployment use the final artifacts/specification; confirm generated docs/examples/links before deployment.
- [ ] Commit and push the reviewed final state directly as requested, publish release/installers/extension/site, and verify the actual public downloads and deployed pages.

Nothing in this checklist authorizes calling an unfinished gate complete merely because time, tokens or usage are running low. No publish or install has occurred at this checkpoint. Keep remaining work here and in RELEASE_HANDOFF.md so continuation does not depend on chat memory.
