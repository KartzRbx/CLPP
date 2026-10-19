# Release status and validated candidate

Updated 2026-10-18.

## Public downloads

The published VM release is **0.11.0**. Use the matching runtime and editor extension from [CL++ VM 0.11.0](https://github.com/KartzRbx/CLPP/releases/tag/v0.11.0). Check each downloaded file against that release's `SHA256SUMS`. The independent Luau compiler has its own release history and executable; it is not the VM.

## Candidate 0.11.1

Candidate **0.11.1** passed platform and final-package validation on 2026-10-18. It has not been published as a GitHub Release. There are no public 0.11.1 download links yet. Examples elsewhere in this site follow the published release unless a candidate-only behavior is explicitly identified.

The exact validated source is `1fd141de376d056e6df8d86d99f8d4a23a02f255`. [Candidate source and documentation](https://github.com/KartzRbx/CLPP/tree/1fd141de376d056e6df8d86d99f8d4a23a02f255) and [candidate release notes](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/.github/releases/v0.11.1.md) are available for review.

### Validated changes in the candidate

- Omitted `optional<T>` object fields default to `null`. Optional presence checks reject `null` and `false`; zero and empty strings remain present. `if`, `guard` and logical operators follow the same rule. See the candidate [optional-value guide](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/docs/guide/08-enums-variants-option-result.md).
- Postfix `value:is<Type>` checks runtime types and supports narrowing, method completion and highlighting. Logical operators retain the CL++ spelling `and` and `or`. See the candidate [language contract](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/docs/reference/language-contract.md).
- Object initializer completion offers imported, inherited and generic fields; hover and navigation identify their declarations. See the candidate [data definitions guide](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/docs/guide/07-structs-and-inheritance.md).
- Safer editor rename, references, document versions, edit validation and external-method highlighting. Formatting and auto-import retain their dedicated validation. See the candidate [editor guide](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/docs/guide/15-tools-and-editor.md).

- Native signal completion now offers `Connect`, `Once`, `Fire`, `Clear` and `Wait`. The first `:` offers these beside `is<Type>` and inserts the missing second colon when accepted. Callback snippets use explicit payload types; connections offer `Disconnect` and `Connected`. The new regression checks cover 16 scenarios, including aliases, imported payloads, empty payloads and scoped variables. Accepted callback snippets preserve source type names such as `H.Human`; acceptance is compiled in the imported-payload test so internal compiler identities cannot silently produce invalid source. An additional 37-case runtime/editor contract covers fields, returned and parenthesized signals, indexed receivers, first-colon acceptance, imported enum cases, inherited callback members, generic payloads and concrete inherited generic signatures. Every accepted callback snippet in that contract is compiled. See the candidate [events guide](https://github.com/KartzRbx/CLPP/blob/1fd141de376d056e6df8d86d99f8d4a23a02f255/docs/guide/13-events.md).

### Platform and package results

[Candidate CI](https://github.com/KartzRbx/CLPP/actions/runs/39187784489) passed all seven jobs. [Documentation validation](https://github.com/KartzRbx/CLPP/actions/runs/39187784536) also passed on the same source revision.

| Platform/build | Compiler checks passed | Additional malformed-input checks passed |
| --- | --- | --- |
| Linux x64 release | 555 | 302 |
| Linux GCC with address/undefined-behavior checks | 555 | 302 |
| macOS ARM64 Clang | 555 | 302 |
| Windows MSVC | 556 | 302 |
| Windows LLVM with address/undefined-behavior checks | 555 | 302 |

The Linux completion matrix passed 10,000 requests with zero failures; p99 was 0.31 ms on that runner. These results describe the tested machines, not a speed guarantee for every computer.

Prepared packages include Windows x64, Linux x64 and macOS ARM64 runtime ZIPs, the Windows executable, VS Code VSIX, a locally built Zed bundle, the Windows installer, licenses and checksums. Final Windows package checks include execution without development SDK paths, new installation, repeated installation, upgrade from the actual public 0.11.0 package, checksum rejection and removal that preserves unrelated user files. Zed installation was checked with isolated settings and the final compiler; its extension remains a local-install package, not a registry publication.

### Remaining publication work and limits

Publish the verified source and artifacts, then change the install instructions and download links together. If the compiler changes, repeat candidate validation before publication. Existing 0.11.0 instructions intentionally remain pinned to available downloads.

macOS Intel was not tested. Native desktop/audio device coverage remains host-specific. Rename/references are limited to current-document variables and parameters; cancellation does not interrupt analysis already in progress. Passing these checks does not prove the absence of every defect. Luau backends, Roblox integrations and application packages retain their independent scope.
