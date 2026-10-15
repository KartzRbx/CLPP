# General CLI examples

These programs use ordinary CL++ syntax and application-independent standard
APIs. They contain no compiler handshake, Roblox profile, project format or
Cluaupp integration. Use a freshly rebuilt VM with the APIs documented in
`docs/reference/cli-host.md`.

- `copy_file.clp SOURCE DESTINATION`: bounded binary copy with atomic replacement
  and a same-file check that detects hard links. Maximum input: 4 MiB. Destination
  directory must exist; invalid arguments/read/write failures return status 1.
- `file_info.clp PATH`: typed metadata serialized as a named JSON object to stdout.
  It reports existence, directory status, byte size and Unix modification time.

Run with `clpp-vm examples/cli/copy_file.clp source.bin generated.bin` or the equivalent
path to your local VM binary. No arguments prints usage without writing files.
The C++ VM executes these scripts; producing a standalone executable for each
program remains separate distribution work.

Atomic replacement gives complete old/new file visibility on supported filesystems;
it does not preserve destination permissions/ACLs, guarantee power-loss durability
or replace application-level input/output policy. Metadata can be used to schedule
checks, but size/timestamp alone cannot prove unchanged content for a correct cache.
Filesystem watchers, hashing and incremental dependency invalidation remain general
tooling work, not language syntax or a built-in application.
