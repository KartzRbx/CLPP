# Release status and validated candidate

Updated 2026-10-17.

## Public downloads

The published VM release is **0.11.0**. Use the matching runtime and editor extension from [CL++ VM 0.11.0](https://github.com/KartzRbx/CLPP/releases/tag/v0.11.0). Check each downloaded file against that release's `SHA256SUMS`. The independent Luau compiler has its own release history and executable; it is not the VM.

## Candidate 0.11.1

Candidate **0.11.1** passed platform and final-package validation on 2026-10-17. It has not been published as a GitHub Release. There are no public 0.11.1 download links yet. Examples elsewhere in this site follow the published release unless a candidate-only behavior is explicitly identified.

The exact validated source is `3cf37a080c771f670cbecc0799b16b8f05600eec`. [Candidate source and documentation](https://github.com/KartzRbx/CLPP/tree/3cf37a080c771f670cbecc0799b16b8f05600eec) and [candidate release notes](https://github.com/KartzRbx/CLPP/blob/3cf37a080c771f670cbecc0799b16b8f05600eec/.github/releases/v0.11.1.md) are available for review.

### Validated changes in the candidate

- Omitted `optional<T>` object fields default to `null`. Optional presence checks reject `null` and `false`; zero and empty strings remain present. `if`, `guard` and logical operators follow the same rule. See the candidate [optional-value guide](https://github.com/KartzRbx/CLPP/blob/3cf37a080c771f670cbecc0799b16b8f05600eec/docs/guide/08-enums-variants-option-result.md).
- Postfix `value:is<Type>` checks runtime types and supports narrowing, method completion and highlighting. Logical operators retain the CL++ spelling `and` and `or`. See the candidate [language contract](https://github.com/KartzRbx/CLPP/blob/3cf37a080c771f670cbecc0799b16b8f05600eec/docs/reference/language-contract.md).
- Object initializer completion offers imported, inherited and generic fields; hover and navigation identify their declarations. See the candidate [data definitions guide](https://github.com/KartzRbx/CLPP/blob/3cf37a080c771f670cbecc0799b16b8f05600eec/docs/guide/07-structs-and-inheritance.md).
- Safer editor rename, references, document versions, edit validation and external-method highlighting. Formatting and auto-import retain their dedicated validation. See the candidate [editor guide](https://github.com/KartzRbx/CLPP/blob/3cf37a080c771f670cbecc0799b16b8f05600eec/docs/guide/15-tools-and-editor.md).

### Platform and package results

[Candidate CI](https://github.com/KartzRbx/CLPP/actions/runs/39045427996) passed all seven jobs. [Documentation validation](https://github.com/KartzRbx/CLPP/actions/runs/39045428005) also passed on the same source revision.

| Platform/build | Compiler checks passed | Additional malformed-input checks passed |
| --- | --- | --- |
| Linux x64 release | 553 | 302 |
| Linux GCC with address/undefined-behavior checks | 553 | 302 |
| macOS ARM64 Clang | 553 | 302 |
| Windows MSVC | 554 | 302 |
| Windows LLVM with address/undefined-behavior checks | 553 | 302 |

The Linux completion matrix passed 10,000 requests with zero failures; p99 was 0.26 ms on that runner. These results describe the tested machines, not a speed guarantee for every computer.

Prepared packages include Windows x64, Linux x64 and macOS ARM64 runtime ZIPs, the Windows executable, VS Code VSIX, a locally built Zed bundle, the Windows installer, licenses and checksums. Final Windows package checks include execution without development SDK paths, new installation, repeated installation, upgrade from the actual public 0.11.0 package, checksum rejection and removal that preserves unrelated user files. Zed installation was checked with isolated settings and the final compiler; its extension remains a local-install package, not a registry publication.

### Remaining publication work and limits

Publish the verified source and artifacts, then change the install instructions and download links together. If the compiler changes, repeat candidate validation before publication. Existing 0.11.0 instructions intentionally remain pinned to available downloads.

macOS Intel was not tested. Native desktop/audio device coverage remains host-specific. Rename/references are limited to current-document variables and parameters; cancellation does not interrupt analysis already in progress. Passing these checks does not prove the absence of every defect. Luau backends, Roblox integrations and application packages retain their independent scope.
