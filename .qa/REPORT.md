# Local language and editor QA — 2026-10-14

The author now authorizes general CLI support, site/installer review, local installation
and GitHub publication after validation. The primary user's Combat edit is preserved.
Historical local-only stages below describe their scope at that time.

## General CLI scope correction and literal-safe formatter — current stage

The Cluaupp-specific prototype, handshake and protocol suite were removed from
source, required tests and public instructions at the author's correction. A
recoverable copy is only in ignored build output. Historical 21-case results
below describe that withdrawn prototype, not a language feature. Generic process,
filesystem and core control-flow fixes remain. The external Cluaupp repository
was not changed.

- Generic Process/Path/Fs APIs now include bounded text/binary reads, atomic
  text/byte writes, typed byte-size/Unix-time metadata, executable discovery,
  canonical paths and existing-file identity. No target-language/project policy.
- The formatter had a real delimiter-loss bug: string/import payloads omitted
  quotes when emitted. It now copies original literal bytes, preserves complete
  nested templates and leaves malformed lexical input untouched. Two unit cases
  passed 143 assertions; real LSP formatting/recompilation/idempotence passed all
  59 example/benchmark sources (build/formatter-corpus-normal.xml).
- Normal integrated tests passed 528/528 (build/generic-cli-normal-ctest.xml).
  Generic storage contracts passed 10/10 in normal and ASan/UBSan builds; general
  process tests passed 11/11 and legacy Unicode/NUL filesystem tests 2/2.
- New storage tests exposed and resolved millisecond timestamp truncation by the
  Windows library path and transient Windows replacement failures while readers
  are open. Native Windows metadata preserves milliseconds; bounded retry keeps
  replacement atomic. Failed writes preserve destinations and remove temp files.
  Canonical-path tests include the actual OneDrive checkout. Initial path failure
  also depended on toolchain/sandbox access; do not infer a universal cloud bug.
- The 50 ms normal Debug/Release LSP gate remains. Instrumented runs now use an
  explicitly separate 500 ms responsiveness gate; their pass cannot be reported
  as a 50 ms performance result. Historical flaky timing measurements remain.
- Isolated CMake install and CPack extraction passed with only system DLLs,
  including Unicode source/arguments, checked expressions and generic file/path
  APIs. Both editor-installer regressions and the presence mapping regression
  passed. VS Code 0.10.5 and Zed installed with the generic APIs/formatter fix.
- Immutable local snapshot manifest: build/validated-cli/latest.json; SHA-256
  7b3fcc5a6cdb2476b7f3c2e111d9fe3efdd08a94ad410926881143816919d313.
  Full instrumented suite, final site integration, upstream integration and
  GitHub platform results are being verified; do not mark them passed yet.

## Historical withdrawn prototype and protected-scope fixes

- Implemented `tools/cluaupp/main.clp`: configuration/argv, explicit external
  executable and argument boundaries, legacy version/manifest handshake, typed
  JSON request/artifact validation, stderr/context and controlled final status.
  The caller selects output; untrusted outputHint is ignored. Existing output is
  preserved on protocol/compilation failure. Atomic writes, filesystem alias
  detection, project/Rojo/profile handling and source-map sidecars remain pending.
- **523/523** normal integrated CTest checks passed in 92.21 s, including the new
  permanent host contract. Evidence: build/cluaupp-normal-ctest.{log,xml}.
- **458 unit cases / 4,505 assertions** passed under ASan/UBSan. Two new cases
  cover nested/recursive/void returns and break/continue/switch protected scopes.
  Return expressions still execute while their own catch is active. Evidence:
  build/cluaupp-all-units-sanitizers.xml; focused build/try-lifetime-normal.xml.
- **21/21** final host/filesystem cases passed in both normal and ASan/UBSan
  builds, after the final source-read guard. Evidence:
  build/cluaupp-host-{normal,sanitizers}.{log,xml}. These tests use a controlled
  protocol peer, not a real Luau backend; rejection of the actual VM is tested.
- Three real findings were fixed: dead try handlers surviving return/break/continue;
  Windows narrow-path operations failing Unicode filenames; embedded NUL paths
  reaching truncated filenames. The compiler now emits EndTry for the scopes
  actually left, filesystem paths use UTF-8, and NUL is rejected before access.
  The old installed binary reproduced the misleading catch diagnostics and
  Unicode failures (build/cluaupp-host-before-fix.xml); a separate isolated old
  binary repro confirmed a NUL-suffixed write modified the truncated filename.
- Initial harness setup needed explicit UTF-8 Python protocol streams and LF
  fixture bytes. A fixture named nul.clp targeted a reserved Windows device,
  yielding an empty program; renamed invalid_paths.clp and verified real NUL
  rejection. These were fixture/setup defects, not extra product findings.
- Current syntax checks cover **58** examples/benchmarks/tool sources; Zed parse,
  query and role checks cover **59** sources, including the host. Evidence:
  build/cluaupp-{sources,zed}.log. Local site build/content/links passed:
  55 sources, 250 CL++ blocks (19 target specs), 387 Markdown links;
  build/cluaupp-site.log. No site deployment occurred.
- VS Code **0.10.4** packaged, extracted-compiler tested and installed; Zed's
  immutable compiler snapshot updated. Installed VS Code compiler SHA-256:
  7999176e310a5417f599dd6c7c15544fa2470062e479f4275afed005bf63b2ac.
  Editor reload and visible UI checks remain. No real compatible Luau compiler
  was available; full migration/backend correctness is not claimed.
- Historical sanitizer latency, cross-platform and broader native gates below
  remain open. The new unit/host results do not replace those gates.

Earlier stages follow as historical evidence.

## Language handoff and examples — additional local stage

- New consolidated public language contract, exact precedence, contextual forms,
  removed syntax and future proposals; all **82 lexer keywords** have classified entries.
- **26/26** dedicated contract checks passed: ten golden entry
  programs, fifteen intentional rejections, keyword/example inventory.
  Evidence: build/language-contract.xml.
- All **57** example/benchmark sources passed current syntax checking.
  Evidence: build/language-sources.log.
- Documentation: **213** executable examples passed, **19** target specs excluded.
  Evidence: build/language-docs-examples.log. No excluded example counted as passing.
- Process host: **11/11** real-child cases passed under Windows ASan/UBSan.
  Evidence: build/tooling-host-sanitizers.{log,xml}. Earlier attempts had a missing
  DLL search path in explicitly copied subprocess environments; configuring PATH
  inside the Python launcher resolved startup. The leader fixture needed raw-string
  escaping to preserve its intended newline. Empty assertion messages now remain
  failures in the harness instead of being counted as passes. No host result was
  altered to hide those setup/fixture failures.
- Site source and build policy passed after adding CLI host docs: **55 sources**,
  **250** CL++ blocks (19 target specs), **387** local Markdown links; generated
  routes/assets exist. This is a local site build, not deployment.
- Other-AI entry: docs/internal/AI_LANGUAGE_HANDOFF.md. Cluaupp migration backlog
  updated with source-level APIs and actual TypeScript compiler contract.
  No completed Luau backend, full Cluaupp port or self-hosting is claimed.

## Checked expressions, DLL startup and Discord image — latest stage

- `(Type expression)` implemented for checked aliases, generic/module types,
  contextual callbacks/arrays, compatible numeric widening and single evaluation.
  Unrelated values and unknown dynamic results cannot be forced into a type.
  Ordinary grouping/calls/constructors and formatter gaps are preserved.
- **520/520** current primary Windows GCC Debug CTest checks passed.
  Evidence: build/type-ascription-normal-ctest.{log,xml}.
- **456 unit cases / 4,499 assertions** passed under Windows ASan/UBSan.
  The focused expression/IDE group passed **4 cases / 20 assertions**.
  Evidence: build/type-ascription-all-units.xml and build/type-ascription.xml.
  This is not a new full CTest sanitizer latency verdict; the historical flaky
  timing gate remains recorded below and enabled.
- ASan executables now stage their matching runtime, libc++ and unwind DLLs.
  Normal MinGW CLI packages link statically. Standalone startup/extracted VSIX
  execution passed without an SDK path. The initial VSIX packaging failure was
  caught before installation; the final package passed and was installed.
- A stale negative fixture treated a statically refined any binding as an unknown
  dynamic source. It now exercises an actual any-returning call. A separate real
  QA setup defect surfaced while reporting that failure: mixed Catch2 sanitizer
  container annotations corrupted XML failure reports. Catch2 now uses the same
  instrumentation as the tests; an intentional assertion failure produces complete
  safe XML. No container-overflow suppression was added. The focused sanitizer
  CTest contracts passed **2/2**. Initial partial evidence is preserved in
  build/type-ascription-initial-report.xml.
- Zed grammar/query/role checks passed across **58** source files. New corpus
  exposed missing single-quote and atomic syntax; both now match the compiler.
  The new capture fixture uses word boundaries to distinguish the type Outcome
  from the variable typedOutcome; its earlier failure was stale positioning.
- VS Code **0.10.3** installed and its compiler hash/current typed expression
  verified outside the checkout with only system paths. SHA-256:
  463c537d4d5c9fbfd42cd26781ff88f1ed936a39327ba181b9fda24078462c2a.
  Zed received the final normal compiler snapshot and new WASM grammar.
- The author's screenshot confirms VS Code presence text already recognizes CL++.
  They chose the CL++ image while retaining the editor activity title. A guarded
  iCrawl 5.9.2 local mapping was installed with original-bundle backup/hashes;
  mapping/other-language/idempotence tests passed. Public logo HEAD returned 200;
  authorized Discord RPC accepted the image and cleared its test activity.
  Actual reloaded UI image rendering is not claimed; reload the editor to apply.
- Website source and generated links passed locally: 55 sources, 250 CL++ blocks
  (19 target), 387 Markdown links. No site deployment or marketplace publication.

## Results

- Fresh Windows LLVM MinGW Debug integration: **515/515 CTest checks passed**.
  Evidence: build/native-review-integration.{log,xml}.
- Windows ASan/UBSan full run before the additional JSON regression: **513/514
  combined checks passed**. The sole failure was the 50 ms completion timing
  assertion; all functional assertions in that test passed. Five isolated timing
  runs produced 3 failures and 2 passes (53.68/61.38/62.13/43.50/43.63 ms).
  This is recorded in flaky.yml; it remains enabled in CI.
- Current instrumented native regression group: **3 cases / 12 assertions passed**,
  including the added JSON test. Do not call this a fresh full 515-case sanitizer run.
- Installer regressions: **2/2 passed**, including immutable compiler snapshots,
  rebuild/reinstall behavior, preserving JSONC/theme and additional presence servers.
- VSIX integrity/extracted compiler passed; installed VS Code extension **0.10.2**
  contains the exact fresh integration compiler hash, grammar and client. The old
  0.8.0 directory was residual: the CLI said that ID was not installed. No active
  unrelated extension was removed. The new extension is installed, not published.
- Presence adapter compiled for wasm32-wasip2. Official upstream v0.12.0 Windows
  asset SHA-256 verified: 6043db62227d22432093175ec971f45763a9adebea2c8c0466ce5f967e2c9f00.
  After explicit author approval, desktop Discord acknowledged the CL++ RPC test
  and its clearing. The real presence LSP accepted initialization, a CL++ document
  and parameterless shutdown. The first test used null params for shutdown and
  was stale for that third-party server; the corrected protocol test passed.
- Zed received the current compiler snapshot and the separate presence adapter.
  The author screenshot confirms color swatches. Public-profile rendering,
  interactive completion/hover and VS Code color-picker UI remain unverified.
  Windows capture failed with FrameArrived timeout; no screenshot claim is made.
- Site rebuilt successfully: **52 pages**, 245 CL++ source blocks (19 target specs),
  264 local Markdown links; generated links/assets passed. Evidence: build/native-review-site.log.
- Debug --check scaling (3 samples including process startup): medians ~134 ms /
  833 ms / 1,709 ms for 1,000 / 10,000 / 20,000 bindings. These local measurements
  are not an optimized Release result or a general speedup claim.

## Findings fixed

1. Large floating numbers were cast to long long before checking their range in
   native text formatting. Text.toNumber("1e300") -> joinAll reproduced UBSan's
   float-to-integer overflow. Range/truncation checks now precede conversion.
2. Joining exact 64-bit integers used their double mirror and lost precision.
   Integer values now use their exact integer field.
3. Native repeat accepted NaN through its numeric guard, then cast it to int.
   Nonfinite counts are rejected first.
4. Numeric parsing accepted a prefix before an embedded NUL and ignored trailing
   bytes. It now requires complete byte consumption.
5. The native JSON stringify indent conversion could overflow int; multiplication
   of indentation by depth also used int. Conversion is range-checked and size
   multiplication uses size_t. Json.Pretty's existing public contract is unchanged.

## Permanent protection and remaining work

- Existing CTest/Catch2/LSP suites retained. New regressions added using their
  conventions. Windows CI now includes the isolated installer checks; remote CI
  has not executed in this session.
- Cluaupp host/compiler/self-hosting requirements, priorities and completion
  criteria are recorded in docs/internal/CLUAUPP_MIGRATION.md. No Luau backend,
  child-process API or standalone source-program executable support is claimed.
- Next native review: remaining integer conversions in window/UI/terminal APIs
  (including clamps applied after casts); reproduce and range-check before any
  native side effect. The text/JSON fixes are not a complete native-package audit.
- Linux/macOS/MSVC, Windows leak detection, broader native/resource review,
  instrumented latency policy and visible editor/profile checks remain gates.
- Dedicated secret/SAST and test-suite mutation tools were unavailable; the
  focused conversion review is not a complete security audit. No HTTP/authz
  layer applies to this local compiler. Live deployment canary is out of scope.

| Layer | Ran | Passed | Real bugs | Flaky | Stale |
| --- | ---: | ---: | ---: | ---: | ---: |
| Normal integrated checks (latest) | 523 | 523 | 3 findings fixed in latest stage | 0 | 0 |
| Unit assertions, ASan/UBSan (latest) | 4505 | 4505 | 0 remaining in these checks | 0 | 0 |
| Cluaupp host, normal and ASan/UBSan (latest) | 42 | 42 | included above | 0 | 2 fixture/setup issues corrected |
| Instrumented integrated checks | 514 | 513 | 0 functional failures | 1 timing check | 0 |
| Instrumented native regressions | 3 | 3 | 5 findings fixed above | 0 | 1 test fixture corrected before passing |
| Installer regressions | 2 | 2 | 0 | 0 | 0 |
| Discord RPC and third-party LSP | 3 | 3 | 0 | 0 | 1 parameterless shutdown corrected |

Verdict: DON'T SHIP — local Windows correctness gates pass, but cross-platform,
visual and instrumented timing gates remain; publication is not authorized.
