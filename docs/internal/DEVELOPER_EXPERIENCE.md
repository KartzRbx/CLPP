# Developer experience follow-up

Keep CL++ syntax and semantics consistent across the compiler, CLI, LSP, VS Code, Zed and documentation. Familiar conveniences must fit the language contract; ecosystem packages stay secondary.

## Current patch

- [x] Omitted optional fields become null, including imports, aliases, inheritance and generic specializations.
- [x] Optional negation and conditions agree: only null and false fail; zero and empty strings remain present.
- [x] Guard, if, loops, ternaries and short-circuit operators evaluate operands once.
- [x] Refinement rejects unsafe reads after assignment and on a failing presence branch.
- [x] Full suite: 548/548; focused ASan/UBSan checks and LSP contracts passed; local VS Code/Zed compiler paths updated.
- [ ] Publish the patch and update the public site after validation.

## Language and editor audit

- [ ] Initializer field completion for imported definitions, inheritance and generic types; filter already supplied fields.
- [ ] Initializer key hover, navigation and semantic coloring resolve to the owning field.
- [ ] Check completion, auto-import, signature help, rename, references and diagnostics on incomplete code.
- [ ] Audit formatting idempotence and preservation of strings, comments and generic brackets.
- [ ] Compare compiler acceptance, editor diagnostics, grammar/highlighting and executable documentation for each supported construct.
- [ ] Exercise errors and recovery, Unicode, module cycles, empty and large files, cancellation and response times.

A checked item records implemented behavior, not proof of zero defects. Public release 0.11.0 predates the current optional-condition patch.
