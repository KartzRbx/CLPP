# Developer experience follow-up

Keep CL++ syntax and semantics consistent across the compiler, CLI, LSP, VS Code, Zed and documentation. Familiar conveniences must fit the language contract; ecosystem packages stay secondary.

## Current patch

- [x] Omitted optional fields become null, including imports, aliases, inheritance and generic specializations.
- [x] Optional negation and conditions agree: only null and false fail; zero and empty strings remain present.
- [x] Guard, if, loops, ternaries and short-circuit operators evaluate operands once.
- [x] Refinement rejects unsafe reads after assignment and on a failing presence branch.
- [x] Full suite: 554/554 after the editor audit; 69 editor cases passed across normal, optimized, ASan/UBSan and installed compiler runs. The earlier 49 initializer checks remain covered; local VS Code/Zed paths updated.
- [x] Postfix `value:is<Type>` with narrowing, runtime checks, method highlighting and `value:` completion; documentation and installed VS Code/Zed updated.
- [ ] Publish the patch and update the public site after validation.

## Language and editor audit

- [x] Initializer field completion for imported definitions, inheritance and generic types; filter already supplied fields.
- [x] Initializer key hover, navigation and semantic coloring resolve to the owning field.
- [x] Check completion, auto-import, signature help, current-document variable/parameter rename, references and diagnostics on incomplete code.
- [x] Audit formatting idempotence and preservation of strings, comments and generic brackets; 75 normal/instrumented cases, including runtime output comparisons.
- [x] Compare compiler acceptance, editor diagnostics, grammar/highlighting and executable documentation through the current construct/keyword inventory and example corpus.
- [x] Exercise errors and recovery, Unicode, module cycles, empty and large files, late cancellation and response times; 10,000 completion scenarios plus a Release p99 <=50 ms gate.

Evidence: [editor audit](../../.qa/editor-audit/REPORT.md). The audit also ran 2,844 adversarial compiler cases and hardened the QA result checker against sanitizer exit-code false positives. Workspace/type/member renaming and interruption of running analysis remain outside the current editor interface; those limits are documented rather than counted as implemented.

A checked item records implemented behavior, not proof of zero defects. Public release 0.11.0 predates the current optional-condition patch.
