# General CLI support and external Cluaupp migration notes

Recorded 2026-10-14 at the author's request. This is a tracked backlog, not a
claim that the migration or a Luau backend already exists. Preserve CL++ syntax,
lexer and current VM semantics. Stabilize the language and editor first; native
package work and the Cluaupp migration follow. Keep public docs/site aligned with
implemented behavior. Remote CI, pushes and publication remain outside the
earlier local-only authorization (superseded by the scope correction below).

## Scope correction

CL++ must support CLI programs in general. No Cluaupp commands, compiler-version
policy, Roblox profiles, Rojo mapping or Luau protocol belongs in its core,
standard library, installer or required tests. The application-specific prototype
and protocol suite were removed from this repository. The generic Process, Path,
JSON, I/O and filesystem APIs and core correctness fixes remain. Migration of
Cluaupp belongs in its own repository. The author now authorizes GitHub publication
after validation; cross-platform/remote checks remain subject to actual access.

## Priority 0: finish the current language gates

- [ ] Verify completion, hover and semantic highlighting visibly in Zed and VS Code.
  The author's screenshot confirms Zed color swatches. A clickable Zed color
  picker is an editor capability, not provided by the CL++ server alone.
- [ ] Separate instrumented correctness results from real editor latency gates.
  The latest ASan/UBSan run passed functional checks but exceeded the 50 ms
  completion budget; isolated repetitions produced both passes and failures.
  Preserve the timing evidence rather than counting the combined run as green.
- [ ] Finish native conversion/resource audits, with sanitizer regressions for
  findings. Text/JSON bounds fixes have regressions; remaining UI/window/terminal
  conversion and resource paths still need review.
- [ ] Run Linux, macOS and MSVC on actual runners when remote work is authorized.
  Windows builds cannot establish these results.
- [ ] Verify distribution installation and downloads on each supported platform.
- [ ] Publish the site/extensions/distribution only when authorized and gates pass.
- [ ] Verify the optional Discord presence visually in the editors/profile.
  Workspace name/file basename sharing was approved and RPC accepted the CL++
  image; actual rendering after editor reload remains unverified.

## Priority 1: reliable CLI host primitives

| Requirement | Current evidence/status | Completion criterion |
| --- | --- | --- |
| Entry and exit status | Top-level statements run; Process.SetExitCode selects successful final status while preserving cleanup; failure remains nonzero. Real parent-process tests passed locally | Define controlled termination and preserve cleanup/error messages; test success/failure through a parent process |
| Arguments | `args()` exists | Exercise `build`, options, Unicode, empty arguments and usage errors |
| Terminal | `post`, Io.ReadLine/ReadAll/EndOfInput exist; Io.Error provides independent stderr, tested locally | Add/document distinct stdout/stderr writes and test EOF, pipes and exit behavior |
| Text files/directories | Fs.read/write/append/list/exists/isDir/remove/makeDir exist; Windows UTF-8 paths and NUL rejection now have real filesystem regressions | Broaden error cases and implement/test binary byte I/O; do not infer byte APIs from text APIs |
| Paths | Source-level Path operations implemented and Unicode CLI/path cases tested on Windows; Fs retains separate policy | Expose normalized relative/absolute paths, joins, Unicode and documented cross-platform behavior |
| Environment | Os.env exists | Expose current directory, temporary directory and executable location; test absence/errors |
| JSON | Typed parsing/stringification implemented | Verify manifests/config/source-map round trips, precision, malformed data and diagnostic context |
| Child processes | Process.Run implemented: explicit executable, argv array, cwd/environment, stdin/separate output, budgets and owned-group cleanup; 11 Windows ASan/UBSan host cases passed | Spawn with argument arrays, explicit working directory/environment, separate stdout/stderr, exit status and cancellation; no shell concatenation |
| Errors | try/catch and diagnostics exist | Preserve original failures and command context throughout CLI layers |
| Collections | Arrays/dictionaries/strings/generic types exist | Test the actual project/cache workloads; assess a set API only if needed |
| Distribution | VM compiler portable packages tested locally | Define script-plus-VM installation; a standalone compiled program executable is a separate unimplemented capability |

Deliverable in CLPP: reliable, documented and tested general CLI capabilities.
Application migration is a consumer of those APIs, owned by the external project.

## External project only: compiler access and CL++ to Luau contract

- [x] Inventory the actual TypeScript Cluaupp repository, commands, tests and
  compatibility requirements before porting it. Do not invent APIs from its name.
- [ ] Choose compiler integration in the external application repository.
  The C++ VM is not a Luau emitter; a project-specific adapter must not be embedded
  in the language to impersonate one.
- [ ] Define the supported CL++ subset and behavior-preservation rules for a Luau
  emitter: references, closures, generics, optional values, arrays, casts and
  defaults, including deliberate unsupported constructs.
- [ ] Expose parser/AST spans and module dependency/cycle results through the chosen API.
- [ ] Return structured diagnostic codes, severity, file and ranges.
- [ ] Return predictable generated files and dependencies; identify compiler,
  backend and contract versions/capabilities.
- [ ] Generate and validate source maps against emitted Luau positions.
- [ ] Receive Roblox registry/profiles, server/client context, authority/thread
  policies and Rojo graph as explicit host inputs, outside the core VM semantics.
- [ ] Test emitted code with real Luau tools and the supported Roblox host adapter.

The current C++ rework's official execution target is its VM. It does not contain
a Luau emitter or Luau source maps. Writing a transpiler in CL++ can emit Luau as
text without changing that compiler target. An independent Rust/Luau tool is not
evidence that the C++ rework provides its backend.

## Priority 3: full development workflow

- [ ] Watch filesystem changes, debounce batches and cancel obsolete builds.
- [ ] Hash content, invalidate dependent cache entries and write outputs atomically.
- [ ] Add a persistent daemon only after measuring startup/reanalysis costs.
- [ ] Implement continuous JSON-RPC framing, cancellation and editor integration
  in the CL++ host if required. The current C++ LSP already handles framing;
  it is not automatically a source-level CL++ transport library.
- [ ] Add concurrency only where measured workloads justify it.
- [ ] Complete reproducible installations and cross-platform compatibility checks.

## Later: self-hosting

- [ ] Port parser, binder/checker and emitter only after the host migration and
  compiler API/behavior tests stabilize. A host written in CL++ that invokes a
  C++ or Rust compiler is not a self-hosted compiler.

Every checked item needs executable evidence and matching documentation. Future
proposals remain marked as future and excluded specs are never counted as passes.

## Current source handoff — 2026-10-14

The actual TypeScript repository is `../Cluaupp`. Its external compiler adapter
checks versions >=0.8.0 and <0.9.0, manifest contract 1, filename tags and
`api compile` stdin/stdout JSON. This VM does not satisfy that Luau contract.
The inventory is complete; backend integration and the full tool port are not.

The misplaced host prototype and its protocol-specific tests were removed.
Generic filesystem regressions are in tests/runtime/filesystem_host.py; process
regressions are in tests/runtime/tooling_host.py. Both are independent of any
compiler handshake, source extension, application or target language.

See [AI language handoff](AI_LANGUAGE_HANDOFF.md), the
[language contract](../reference/language-contract.md),
[CLI host signatures](../reference/cli-host.md), and
[executable corpus](../../examples/language/README.md). The dedicated corpus
passed 26 checks (10 golden programs, 15 rejected programs and inventory).
Process/Path APIs remain unverified on Linux/macOS and require rebuilding the
source compiler before older editor installations can use them.
