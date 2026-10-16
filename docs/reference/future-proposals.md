# Future proposals

The author chose to preserve the current language on 2026-10-14. The following
three illustrative contracts are future proposals, not features required for
the current language release. Their handbook blocks remain marked as target
specifications and are not counted as executable tests.

| Proposal | Current language | Decision still needed before adoption |
| --- | --- | --- |
| Protected instance call with `:` | Use ordinary member calls and explicit `try/catch` | Result type, captured error and continuation behavior |
| `destructure` declaration | Use typed tuple values and indexes | Binding syntax, arity, types and mutation rules |
| Missing dictionary key returning `optional<T>` | Absent keys raise a checked runtime failure; use `try/catch` | Lookup result type and compatibility with existing indexing |

Do not introduce compatibility spellings or change existing indexing to implement
these proposals implicitly. Adoption requires a separate language decision,
compiler/editor changes and executable behavior tests.

Native package review and application-specific server/client or Roblox adapter
contracts are separate work. A host example does not add an official engine
backend or service to the CL++ VM. See [implementation status](status.md).
