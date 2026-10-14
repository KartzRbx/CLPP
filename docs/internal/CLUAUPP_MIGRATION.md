# Cluaupp migration to CL++

Recorded 2026-10-14 at the author's request. This is a tracked backlog, not a
claim that the migration or a Luau backend already exists. Preserve CL++ syntax,
lexer and current VM semantics. Stabilize the language and editor first; native
package work and the Cluaupp migration follow. Keep public docs/site aligned with
implemented behavior. Remote CI, pushes and publication remain outside the
current local-only authorization.

## Priority 0: finish the current language gates

- [ ] Verify completion, hover and semantic highlighting visibly in Zed and VS Code.
  The author's screenshot confirms Zed color swatches. A clickable Zed color
  picker is an editor capability, not provided by the CL++ server alone.
- [ ] Separate instrumented correctness results from real editor latency gates.
  The latest ASan/UBSan run passed functional checks but exceeded the 50 ms
  completion budget; isolated repetitions produced both passes and failures.
  Preserve the timing evidence rather than counting the combined run as green.
- [ ] Finish native conversion/resource audits, with sanitizer regressions for
  findings. Text and JSON native integer conversions are the current focus.
- [ ] Run Linux, macOS and MSVC on actual runners when remote work is authorized.
  Windows builds cannot establish these results.
- [ ] Verify distribution installation and downloads on each supported platform.
- [ ] Publish the site/extensions/distribution only when authorized and gates pass.
- [ ] Activate and verify the optional Discord presence only after approval of
  workspace name/file basename sharing with Discord. It is prepared separately
  from the compiler; no token or Client Secret is needed.

## Priority 1: reliable CLI host primitives

| Requirement | Current evidence/status | Completion criterion |
| --- | --- | --- |
| Entry and exit status | Top-level statements run; compiler/runtime errors return a failure status. No source-level controlled exit-code API | Define controlled termination and preserve cleanup/error messages; test success/failure through a parent process |
| Arguments | `args()` exists | Exercise `build`, options, Unicode, empty arguments and usage errors |
| Terminal | `post`, Io.ReadLine/ReadAll/EndOfInput exist | Add/document distinct stdout/stderr writes and test EOF, pipes and exit behavior |
| Text files/directories | Fs.read/write/append/list/exists/isDir/remove/makeDir exist | Verify errors, Unicode and binary byte I/O; do not infer byte APIs from text APIs |
| Paths | Internal C++ resolution exists; Fs currently rejects paths containing `..` | Expose normalized relative/absolute paths, joins, Unicode and documented cross-platform behavior |
| Environment | Os.env exists | Expose current directory, temporary directory and executable location; test absence/errors |
| JSON | Typed parsing/stringification implemented | Verify manifests/config/source-map round trips, precision, malformed data and diagnostic context |
| Child processes | Not exposed to source CL++ | Spawn with argument arrays, explicit working directory/environment, separate stdout/stderr, exit status and cancellation; no shell concatenation |
| Errors | try/catch and diagnostics exist | Preserve original failures and command context throughout CLI layers |
| Collections | Arrays/dictionaries/strings/generic types exist | Test the actual project/cache workloads; assess a set API only if needed |
| Distribution | VM compiler portable packages tested locally | Define script-plus-VM installation; a standalone compiled program executable is a separate unimplemented capability |

First deliverable: a CL++ `cluaupp build` host that reads configuration, calls an
explicitly selected external compiler with a working Luau emitter, writes output
and returns the correct exit status. Keep the TypeScript tool working during the
transition. Compare outputs and failures against its existing tests.

## Priority 2: compiler access and CL++ to Luau contract

- [ ] Inventory the actual TypeScript Cluaupp repository, commands, tests and
  compatibility requirements before porting it. Do not invent APIs from its name.
- [ ] Choose external-process integration first or expose a versioned compiler API.
  The existing parser/checker are C++ libraries; source CL++ cannot invoke them yet.
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
