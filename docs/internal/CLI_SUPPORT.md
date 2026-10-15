# General CLI support and publication checklist

The author's latest correction takes precedence over earlier migration notes:
CLPP supplies general-purpose CLI capabilities. Cluaupp commands, Luau/Roblox
backends, profiles, Rojo mappings, source maps and project policy belong in the
external application repository. No Cluaupp-specific prototype or required test
remains in the language checkout. The independent Luau compiler in upstream main
must remain separate from the C++ VM executable.

## Current source capabilities

- Arguments, stdout/stderr, stdin/EOF and controlled status with resource cleanup.
- Explicit child executable and argument arrays, cwd/environment, independent
  capture, bounded input/output, timeout and owned child-group cleanup.
- Paths, executable discovery, canonical existing-prefix resolution and existing
  file identity including hard links.
- Bounded text and binary reads with catchable failures, atomic text/byte
  replacement, UTF-8 paths, NUL rejection and typed size/time/existence metadata.
- Typed JSON and ordinary collections/closures/modules for application code.
- Generic examples: examples/cli/copy_file.clp and file_info.clp. Public contracts:
  docs/reference/cli-host.md. Tests use real local files/processes, no compiler
  handshake or Roblox-specific fake.

The local immutable validated snapshot is recorded in
`build/validated-cli/latest.json` (ignored build artifact). Read its compilerPath,
SHA-256, capability list and evidence rather than using a binary currently being
rebuilt. This prevents linker locks and mid-build changes in consuming tools.
The local VS Code 0.10.6 package includes these APIs and the layout/literal-safe formatter;
reload an existing editor host before relying on it. No Cluaupp file was edited.

## Remaining general infrastructure

- [ ] Filesystem watching, cancellation and bounded event queues.
- [ ] Content hashing and examples of correct cache invalidation; metadata alone
  is not proof that content is unchanged.
- [ ] General dependency/analysis API or versioned IR for external developer tools.
  The source language does not expose the existing C++ parser/checker yet.
- [ ] Standalone source-program executable generation and associated packaging.
- [ ] Validate new native primitives on actual Linux/macOS/MSVC runners.
- [ ] Broader native package/resource review remains separate from this CLI slice.

## Requested delivery gates

- [ ] Review public site sources, syntax examples, generated routes/assets and
  installation/version guidance; build the final integrated site.
- [ ] Review CMake/CPack and editor installers; exercise isolated installation,
  archive extraction, SDK-independent startup and generic CLI functionality.
- [ ] Run normal and instrumented correctness suites. Normal LSP latency keeps
  the original 50 ms gate; ASan/UBSan uses a separately labelled 500 ms functional
  responsiveness bound, not a deployed-performance claim.
- [ ] Install the validated generic compiler/editor snapshots locally, retaining
  independent Luau installations and user settings.
- [ ] Integrate upstream main without overwriting its independent compiler/docs.
- [x] Publish a reviewable GitHub branch/PR: https://github.com/KartzRbx/CLPP/pull/13.
- [ ] Inspect final GitHub CI results before
  claiming supported-platform or release readiness. GitHub publication is now
  explicitly authorized. Unrelated user Combat edits are not part of the agent commit.

Finite tests can support a release decision; they cannot establish that no bug
exists. Record exact passing/failing/skipped gates and do not convert mock tests
or excluded proposals into claims of implemented behavior.
