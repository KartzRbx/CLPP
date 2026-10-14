# Core and distribution validation gates

Updated 2026-10-14. Priority: core syntax/semantics and IDEs, robustness and
performance, distribution, then embedded/native packages. This inventory
separates implementation evidence from proposed or host-dependent contracts.

## Completed in the latest core stage

- Composed generic aliases: unique union evidence, consistent intersections,
  literal/concrete alternatives, rejection of ambiguity/conflicts and IDE results.
- JSON-RPC field validation: malformed requests receive errors without aborting
  the server; a following valid request succeeds.
- Bounded transport: 16 KiB headers, 64 MiB bodies, checked decimal lengths,
  no duplicate lengths and no unbounded advertised allocations.
- Parser recursion budgets: excessive parentheses, unary chains, conditional
  bodies and generic annotations are rejected. An actual Windows stack-overflow
  reproducer with 128 parentheses now receives a diagnostic. 64 groups remain
  supported. The limit applies to active parser entries, not runtime calls.
- Deterministic malformed-input matrix: 299 subprocess cases passed, including
  operator/member chains. Expression trees have an independent budget of 128
  edges; the prior 256-term operator-chain stack-overflow reproducer is rejected.
- Contextual evidence remains available while unrelated generic field callbacks
  still have provisional signatures. Their final bodies are still checked; an
  invalid generic default is rejected even when an event handler is valid.
- Zed coverage now includes stored generic function values, `decltype`, buffer
  and vector roles and compiler-valid generic field defaults. Its smoke fixture
  is checked by the real compiler as well as the editor grammar.
- Executable documentation: 211 examples passed; target exclusions decreased
  from 21 to 19 by exercising member calls and cooperative event suspension.
- Release Windows build with static C++/thread runtimes: starts with only Windows
  system DLLs on PATH. Extracted VSIX compiler runs current comments/generic aliases.
- CI now tests Release and enables actual ASan plus UBSan in its sanitizer preset.
  Added hostile-input reports and a Windows extracted-package test. These workflow
  changes have not been run on remote runners in this session.

## The 19 excluded specification blocks

On 2026-10-14 the author explicitly deferred protected colon calls, destructuring
and optional missing-key lookup. These three blocks are future proposals, not
unmet requirements for the current language release. Preserve their target
markers without counting them as executed tests.

| Group | Blocks | Gate |
| --- | ---: | --- |
| Audio, console and input packages | 4 | Native effects; review last |
| Conversion APIs | 1 | Native/builtin conversion contract |
| Optional missing dictionary lookup | 1 | Proposed contract differs from current checked runtime exception |
| Protected colon calls | 1 | Proposed spelling/error-result contract, not a supported call form |
| Destructuring declaration | 1 | Proposed syntax; typed tuples/indexes exist, this declaration does not |
| Context/server/client/parallel policies | 5 | Application or transpiler policies, including the reference duplicate |
| Roblox/Cluaupp adapter examples | 6 | Host interfaces; not evidence of Roblox services built into the VM |

Do not silently remove target markers, simulate host effects as language features,
or count different syntax as validation of the proposed contract. Any adopted
contract needs implementation, executable behavior tests and reference updates.

## Remaining verification

Visual Zed/VS Code checks (including color swatches), broader AST/input resource
audits and optimized-runtime profiling remain. A finite fuzz corpus and passing
tests are not proof that no vulnerability exists. The Windows MinGW ASan probe
cannot link because matching clang runtime archives are absent; Linux/macOS/
MSVC and sanitizer jobs need actual runners. A working archive smoke test does
not establish Marketplace/registry installation or public download integrity.
No release has been published, and no checkout changes have been committed or
pushed as part of this stage.

Performance logs use a local Windows Release build at `-O2` with three samples.
Compilation includes process startup. Measured medians for 100/1,000/10,000
bindings were approximately 44/83/266 ms on this run; host load varied. The five
runtime outputs matched Python, but whole-program times were slower than Python
in this run. Do not claim a general speed advantage from these measurements.
