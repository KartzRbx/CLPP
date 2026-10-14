# Local language and editor QA — 2026-10-14

The author's current scope is local validation. No push, remote runner, release
or site deployment ran. The primary user's Combat edit is preserved.

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
| Normal integrated checks | 515 | 515 | 0 remaining in these checks | 0 | 0 |
| Instrumented integrated checks | 514 | 513 | 0 functional failures | 1 timing check | 0 |
| Instrumented native regressions | 3 | 3 | 5 findings fixed above | 0 | 1 test fixture corrected before passing |
| Installer regressions | 2 | 2 | 0 | 0 | 0 |
| Discord RPC and third-party LSP | 3 | 3 | 0 | 0 | 1 parameterless shutdown corrected |

Verdict: DON'T SHIP — local Windows correctness gates pass, but cross-platform,
visual and instrumented timing gates remain; publication is not authorized.
