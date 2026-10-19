# Postfix runtime type tests — 2026-10-17

Verdict: SHIP for local development use. The installed VS Code extension and Zed compiler were updated. Public release 0.11.0 and the deployed site have not been replaced by this development patch.

`value:is<Type>` returns bool without call parentheses. Operands are evaluated once. Tests recognize supported primitive runtime kinds, imported nominal definitions, aliases, inheritance, optionals and supported unions. A proven matching path refines a local in if/guard and short-circuit and/or conditions. Writes invalidate refinement. Erased generic, enum, signal and collection identities are diagnosed. Untagged native objects cannot be identified nominally through any. New bytecode uses format 13; old compiled files require recompilation.

The user's `kartz:is<H.Human> and kartz.cpf == null` succeeds for Human and skips the member access for a non-Human value. Structs with identical fields retain distinct nominal identities. Tests cover primitives, null, false, zero, literal operands, inherited/imported definitions, aliases, malformed syntax, incompatible targets, invalidated refinement, single evaluation and CLIR serialization.

The first highlight implementation classified the intrinsic as an operator. The follow-up changes both LSP semantic roles and TextMate fallback to method/member-function. Italian Pizza's method color is #5A8D7E; no theme override was introduced. Completion after value:, value:i and value:is supplies a Method item with an editable is<Type> snippet and no parentheses. Existing colon triggers are reused.

| Layer | Ran | Passed | Real bugs | Flaky | Stale |
|---|---:|---:|---:|---:|---:|---:|
| Full normal CTest suite | 551 | 551 | 0 remaining | 0 | 0 |
| Focused runtime type-test JUnit checks | 20 | 20 | 0 | 0 | 0 |
| Focused ASan/UBSan type-test and optional checks | 36 | 36 | 0 | 0 | 0 |
| Latest normal/optimized/installed LSP contract | 3 | 3 | 0 | 0 | 0 |
| Latest instrumented LSP contract | 1 | 1 | 0 | 0 | 0 |
| Zed grammar, query and semantic-role contract | 1 | 1 | 0 | 0 | 0 |
| VSIX archive and extracted system-only runtime | 1 | 1 | 0 | 0 | 0 |
| Documentation content/link validation | 1 | 1 | 0 | 0 | 0 |

The full suite covers the completed VM implementation; subsequent highlight and completion-only changes were verified with the current compiler through real LSP, including the installed binary and ASan/UBSan build. Documentation validation counted 55 sources, 252 CL++ blocks (19 explicit targets) and 390 local Markdown links. Documentation examples also passed in CTest.

Installed compiler SHA-256: 2fe594d84172212213cb65e8e0d94e66740524acf68872705d3ca380871a87f1. Snapshot: C:\Users\gtofr\OneDrive\Documentos\GitHub\CLPP_Rework\build\validated-cli\2fe594d84172212213cb65e8e0d94e66740524acf68872705d3ca380871a87f1\clpp.exe. VS Code extension: C:\Users\gtofr\.vscode\extensions\kartzrbx.clpp-language-0.11.0. Settings and binaries are backed up or installed through immutable snapshots. Reload the VS Code window and Zed to activate extension grammar changes in existing windows. Visible pixel colors were not captured; semantic roles, theme mapping and installed bytes were verified.

Evidence: build/type-test-full-final.xml, build/type-test-unit.xml, build/type-test-asan-unit.xml, build/type-test-completion-lsp.log, build/type-test-installed-lsp.log, build/type-test-asan-lsp.log, build/type-test-zed.log, build/type-test-doc-content.log, build/type-test-editor-install.json. Interrupted power-loss runs were discarded. Initial probe fixtures were corrected to use existing alias and binding rules. Windows sanitizers used detect_leaks=0. These checks do not claim zero defects or platform validation of this patch beyond Windows.
