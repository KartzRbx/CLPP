# Optional fields and conditions — 2026-10-16

Verdict: SHIP for local development validation. Public release 0.11.0 is unchanged; patch publication and the wider developer-experience audit remain pending.

The installed release rejected `guard (target.cpf)` for an `optional<number>`. It also required explicitly initializing optional fields. The patch supplies null only for omitted optional fields (after generic/alias/inheritance substitution), and accepts optional conditions and negation consistently: null and boolean false fail; zero and empty strings remain present. Short-circuit evaluation and single evaluation of calls are preserved. Required fields and unsafe payload reads are still rejected.

| Layer | Ran | Passed | Real bugs | Flaky | Stale |
|---|---:|---:|---:|---:|---:|---:|
| Full normal CTest suite | 548 | 548 | 0 remaining | 0 | 0 |
| Focused ASan/UBSan unit checks (JUnit entries) | 43 | 43 | 0 | 0 | 0 |
| ASan/UBSan LSP transport and end-to-end contracts | 2 | 2 | 0 | 0 | 0 |
| Optimized compiler LSP contract | 1 | 1 | 0 | 0 | 0 |
| User HumanState.clp check with system-only PATH | 1 | 1 | 0 | 0 | 0 |

Three new regression cases cover omitted optional fields, negation, and conditions across guards, loops, ternaries and logical operands. LSP tests assert both acceptance of the user's pattern and errors after invalidated refinement. Full suite includes documentation examples, auto-import, formatting, distribution and stress checks.

Local editor snapshot SHA-256: ef31821c168b9b5f82c567debaf39ab7efee8bb4294e5aa8f2aad73cc8cab7a3. VS Code workspace serverPath and Zed LSP use this immutable snapshot. Public binaries and tag 0.11.0 were not replaced. Zed requires a reload; VS Code watches serverPath changes.

Evidence: build/optional-full.xml, build/optional-asan-unit.xml, build/optional-asan.xml, build/optional-release-lsp.log, build/optional-user-example.log, build/optional-editor-install.json. Sanitizers use detect_leaks=0 on Windows. This patch does not add persistent object-field refinement, arbitrary return-path refinement, or all conveniences of other languages. Initializer-key completion and the broader audit are tracked in docs/internal/DEVELOPER_EXPERIENCE.md.
