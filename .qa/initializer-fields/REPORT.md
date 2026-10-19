# Initializer field editor QA

Date: 2026-10-17. Environment: Windows local development build. Public 0.11.0 is unchanged.

## Behavior

`H.Human { na... }` offers the owning definition's accessible fields. Already supplied fields are filtered; editing a key retains that field. Inherited and aliased generic constructors display concrete field types. Initializer keys resolve hover and definition to their declaring field, including imported base definitions, and use the property semantic category. Both server and VS Code trigger completion on `{` and `,`.

## Root cause and regression protection

The editor previously treated initializer keys as ordinary identifiers. It did not use constructor context to select fields, specialize generic types or resolve ownership. The new bounded lexical context index survives unfinished keys, nested values and incomplete calls; semantic field lookup uses the existing checker. Imported type spellings are normalized through the module identity map before querying the checker. This also prevents a same-named local from overriding a valid field key's hover/navigation.

Permanent protocol coverage is in `tests/lsp/test_initializers.py`, registered in CTest. The VS Code provider regression checks replacement of only the partial key. Cases cover public/private fields, supplied keys, imported aliases, nested generic arguments, inheritance, generic function scopes, nested constructors, Unicode, values, calls, comments, strings and declarations.

## Evidence

- Full CTest: 552/552, `build/initializer-full.xml`; includes compiler/VM, formatter, auto-import, documentation, packaging and isolated Windows installer checks.
- Focused initializer protocol: 49/49 in normal, optimized, ASan/UBSan and actual installed VS Code compiler runs.
- VS Code provider suite: all checks passed, including initializer prefix replacement and existing formatting/color/auto-import behavior.
- Documentation policy: 55 sources, 252 CL++ blocks, 19 future target specifications, 390 local Markdown links.
- JavaScript syntax and owned-file diff checks passed.
- Local VS Code VSIX packaged, extracted runtime tested with only system DLLs, installed and compared with source extension.js. Zed compiler updated. SHA-256: `0cc6a62281dbb31c2a6ed9e3afdba39bd753d5a757bf0540c57b8e04d0f4d8f6`.

## Limits

This change has not run a new Linux/macOS/MSVC CI matrix and has not been published. The documentation source is updated; the public site awaits the patch release. Automated provider/protocol checks do not replace a visual check in an already-open editor window; reload VS Code or restart Zed to activate the installed update. Windows LeakSanitizer is disabled. Dedicated SAST scanners were not run for this focused patch. Broader language/editor audit items remain tracked in `docs/internal/DEVELOPER_EXPERIENCE.md`.

| Layer | Ran | Passed | Real bugs | Flaky | Stale |
|---|---|---|---|---|---|
| Full local CTest | 552 | 552 | 0 unresolved | 0 | 0 |
| Initializer protocol, four compiler configurations | 196 | 196 | 0 unresolved | 0 | 0 |
| VS Code providers, packaging, docs and static checks | yes | all | 0 unresolved | 0 | 0 |

Verdict: SHIP for the local initializer editor fix; public release remains pending the broader release gates.
Real bugs: initializer field context and imported type normalization fixed, with permanent regression coverage.
