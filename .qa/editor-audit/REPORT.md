# Editor review and expanded QA

Date: 2026-10-17. Scope: local Windows compiler/editor candidate; public 0.11.0 remains unchanged.

## Result

The editor review and expanded local QA are complete. Ten defects in the tested editor/QA paths were fixed and guarded. The final local CTest run passed 554/554. No unresolved functional failures or quarantined tests remain in this audited scope. This is not a public-release or zero-defect claim.

## Fixed findings

1. **Unresolved references in incomplete code — fixed.** Repro: An unresolved name or out-of-scope name could share slot zero with the first valid local/parameter, corrupting references and rename edits. Fix: Exclude failed entry-buffer bindings from the semantic snapshot, while preserving valid and imported bindings.
2. **Invalid rename identifiers — fixed.** Repro: Renaming score to if, an empty name, a dotted name or pasted statements returned edits that broke the program. Fix: Validate with the real lexer; return InvalidParams without edits.
3. **Rename capture/collisions — fixed.** Repro: Renaming target to taken could bind references to an existing variable or introduce duplicate declarations. Fix: Reanalyze the proposed edits and compare all binding identities, including unrelated references.
4. **Hidden rename rejection in VS Code — fixed.** Repro: The client logged a rejected rename and silently returned undefined. Fix: Propagate rename errors to the editor so the user sees the reason.
5. **Reference declaration filter — fixed.** Repro: References included the declaration even when includeDeclaration was false. Fix: Honor the request filter while keeping rename edits complete.
6. **Stale document updates — fixed.** Repro: A notification with an older version replaced current source. Fix: Ignore non-increasing versions; preserve unversioned compatibility notifications.
7. **Invalid incremental edit batches — fixed.** Repro: A reversed range could replace the rest of the source, and later malformed changes could leave partial edits. Fix: Apply changes transactionally; reject reversed ranges without changing the live document.
8. **Malformed request classification — fixed.** Repro: Wrong-shaped positions or names were returned as InternalError with JSON-library internals. Fix: Return InvalidParams and keep the next request usable.
9. **External method highlighting — fixed.** Repro: number Point::Sum() classified Point as a function in TextMate fallback. Fix: Distinguish owner type and method, including generic owners; keep expression keywords out of declarations.
10. **Adversarial QA false pass — fixed.** Repro: A sanitizer report with exit code 1 could be counted as successful compiler rejection. Fix: Reject sanitizer diagnostics explicitly; regression-test crash/sanitizer/output classification and revalidate archived results.

## Evidence

| Area | Checks and result |
|---|---|
| Full local compiler/VM/tooling suite | 554/554, build/editor-audit-release-gate.xml |
| Editor audit | 69/69 in normal, optimized, ASan/UBSan and installed compiler runs |
| Completion recovery matrix | 10,000 scenarios, 0 functional failures; additional p99 gate passed |
| Formatter | 75/75 normal and ASan/UBSan; idempotence, indentation, runtime output, literal/comment preservation and example corpus |
| Adversarial compiler cases | 2,100 normal + 564 instrumented cases; all archived results reclassified with the hardened checker |
| Fresh instrumented mutations | 180/180 with seed 20261017, including 128 new seeded mutations |
| QA checker itself | 4 test methods, including sanitizer exit 1 and crash-code counterexamples |
| TextMate fallback | 33 real Oniguruma classifications, including external method owners and :is |
| Zed | 62 sources parsed; all queries valid; tested compiler/syntax roles agree |
| VS Code package | Actual installed extension providers, compiler protocol and packaged grammar passed; content identity checked |
| Documentation/site | 215 executable examples checked, 0 failure(s); 19 target-specification blocks excluded (not compiler tests); 55 sources, 252 CL++ blocks, 390 local links; site built and generated assets/routes checked |
| Static/configuration | Native builds, JavaScript syntax, grammar JSON, workflow YAML and owned-file whitespace checks passed |

## Response time and test stability

Optimized matrix: median 0.61 ms, p95 1.42 ms, p99 4.03 ms; p99 target is 50 ms. Maximum was 250.94 ms, with 5 ordinary samples above their 50 ms individual target. Large-file cold completion has a separate 8,000 ms budget. These results do not promise every request finishes within 50 ms.

Separate 10,000-binding hover run: `METRIC 10000 bindings cold_ms=141.38 warm_median_ms=16.02 warm_max_ms=27.30`. These are local measurements, not a cross-machine speed claim.

One Debug large-file maximum-only assertion failed at 515.61 ms while other work ran. Five unchanged isolated repetitions then passed (logs editor-audit-latency-repeat-1..5). Classified as an unstable timing test, fixed rather than quarantined: CTest runs this audit serially; the Debug/instrumented smoke bounds sustained median delay (<500 ms) and stalls (<2,000 ms), while optimized performance retains the separate 10,000-scenario p99 <=50 ms gate. Initial new fixtures also required a terminating guard branch and waiting for asynchronous provider registration; those were test setup corrections, not product defects.

## Permanent checks

The editor audit and QA-checker regressions are registered in CTest. Existing formatter coverage now compares runtime results before/after formatting and checks the language-example expected outputs. Core and completion generators emit JUnit. The existing CI runs the editor/formatter contracts on its configured platforms; Linux Release additionally runs the 10,000-case completion/p99 gate. This CI configuration was validated locally but not executed remotely in this task.

## Installation and remaining boundaries

VS Code and Zed local compiler snapshots updated; SHA-256 `80f145e9640305c744aaf59418a9888d9a0fbe4704d94e740410dd16980903cf`. VS Code must reload its window and Zed must restart/reload extensions to pick up the update in an already-open session.

References/Rename cover current-document variables and parameters, including captures and shadows. Workspace/type/member renaming remains outside that refactoring interface. The synchronous server safely handles late cancellation but does not interrupt an analysis already running. Protocol/provider checks used the real installed extension with a mocked editor API; no new visual UI screenshot was captured. Dedicated SAST scanners, Windows leak detection and a new remote Linux/macOS/MSVC matrix were not run. Native package/device coverage remains deferred. Expanded adversarial runs preceded the final semantic-reference-only fix; final candidate CTest covers the core cases and the new unresolved-reference regressions. The 19 target-specification documentation blocks remain excluded rather than counted as implemented. Public patch publication, release packaging and public site deployment remain separate release work.

| Layer | Ran | Passed | Real bugs | Flaky | Stale |
|---|---|---|---|---|---|
| Full local CTest | 554 | 554 | 0 unresolved | 0 active | 0 |
| Editor contracts | 69 per configuration | all | 0 unresolved | 1 timing assertion fixed | 0 |
| Completion matrix + p99 gate | 10,001 | 10,001 | 0 unresolved | 0 | 0 |
| Adversarial executions | 2,844 | 2,844 | QA false-pass fixed | 0 | 0 |
| Formatter | 75 per configuration | all | 0 unresolved | 0 | 0 |

Verdict: SHIP for the audited local editor patch; remote release gates and publication remain pending.
