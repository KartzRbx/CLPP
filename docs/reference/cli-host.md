# CLI host APIs for development tools

These APIs are implemented in the current source checkout for tools written in
CL++. They require rebuilding the C++ VM; an older installed editor compiler may
not contain them. They are application-independent host APIs.
Windows process/Unicode behavior has a dedicated real-child-process test harness;
Linux/macOS execution still requires validation on those platforms.

## Arguments, terminal and controlled status

`args()` returns the script's argument array. Top-level statements are the entry.
`post(value)` and `Io.Print(value)` print a normal record to stdout;
`Io.Error(text)` writes stderr text independently. `Io.ReadLine()`, `ReadAll()`
and `EndOfInput()` provide the existing input/EOF operations; see
[console I/O](../guide/19-audio-console-automation.md).

```clp fragment
link @clpp.io as Io;
link @clpp.process as Process;
Io.Error("invalid command\n");
Process.SetExitCode(2);
```

`SetExitCode(number)` accepts an integer from 0 through 255. It selects the final
status after successful execution; it does not stop the current statement stream.
Workers finish and resources are cleaned up. Unhandled compiler/runtime errors
remain failures. Do not assume `SetExitCode` is an immediate `exit()` operation.

## Processes

Import `link @clpp.process as Process;`. Public records and operations:

```clp fragment
export def EnvironmentEntry { string name; string value; }
export def Options {
  string workingDirectory = "";
  string input = "";
  number timeoutMs = 60000;
  number maxOutputBytes = 16777216;
  array<EnvironmentEntry> environment = array<EnvironmentEntry>();
  bool inheritEnvironment = true;
}
export def Result {
  number exitCode;
  string stdout;
  string stderr;
  bool timedOut;
  bool outputLimited;
}
```

`Process.Run(string executable, array<string> arguments, Process.Options options)`
returns `Process.Result`. Pass all three parameters; defaults belong to the
options fields. The executable must be an explicit path to a regular executable
file. There is no implicit shell or PATH lookup. Relative executable paths resolve
against the selected working directory. Arguments retain their individual
boundaries, including empty strings, spaces, quotes and shell metacharacters.

```clp fragment
link @clpp.process as Process;
Process.Result result = Process.Run(
  executablePath,
  array<string>("--version"),
  Process.Options { workingDirectory = workingDirectory }
);
if (result.timedOut or result.outputLimited) { report("child process did not complete"); }
if (result.exitCode != 0) { report(result.stderr); }
```

The caller supplies the executable, working directory and individual arguments.
The library contains no application-specific command or protocol.

`Run` blocks while concurrently feeding stdin and capturing stdout/stderr. It
closes child stdin after input is sent and handles a child closing input early.
Each stream preserves its bytes, including CRLF; there is no newline normalization.
A child returning a nonzero status is a result, not an automatic exception.
Invalid options, missing executable paths and spawn failures raise catchable errors.

Timeout must be an integral 1 through 86,400,000 milliseconds. Output budget must
be an integral 1 through 67,108,864 bytes **per captured stream**. Input has a
64 MiB limit. A timeout or exhausted output budget stops the owned process
group/job, returns available bounded output, sets the corresponding flag and
uses `exitCode = -1`. Normal leader completion also cleans up owned descendants
that would retain pipes. This is lifecycle management of the owned group, not a
security sandbox against a deliberately escaping executable.

Empty workingDirectory inherits the host's directory; other values select the
child directory without changing the parent. Environment entries override
inherited values when inheritEnvironment is true; false supplies only explicit
entries. Names cannot be empty or contain `=`/NUL; values and arguments cannot
contain NUL. Supply platform/runtime-required environment variables when choosing
an isolated environment. Async calls do not automatically make this native call
nonblocking; use explicit worker execution if the workload needs it.

## Paths

Import `link @clpp.path as Path;`:

| Operation | Parameters | Result |
| --- | --- | --- |
| CurrentDirectory | none | string |
| TempDirectory | none | string |
| Normalize | string value | string |
| Join | string base, string value | string |
| Absolute | string value, string base | string |
| Parent | string value | string |
| FileName | string value | string |
| IsAbsolute | string value | bool |
| Canonical | string value | string |
| Equivalent | string first, string second | bool |

Paths use UTF-8 strings and native platform path rules. Results use generic
separators. Normalization is lexical: it does not prove existence, resolve
symlinks or enforce a security boundary. Joining an absolute second argument
follows native filesystem path composition. Absolute resolves relative input
against its base; an empty base uses the current directory. None changes cwd.
Filesystem operations retain their separate restrictions; obtaining a normalized
path does not bypass `Fs` policy. Source imports resolve relative to their source
file, independently of this host path API.

`Canonical` returns an absolute normalized path after resolving the existing
prefix, including links; missing trailing components are permitted. Windows uses
real file handles so cloud-provider reparse points are not mistaken for symbolic
links. `Equivalent` compares existing filesystem identities, including hard links;
both paths must exist, otherwise it raises an error. Neither operation prevents
another process changing paths after the check; they are not a filesystem jail.

`Process.ExecutablePath()` returns the actual VM executable path on Windows,
Linux and macOS, independently of the working directory and script filename.

## Integration checks

`tests/runtime/tooling_host.py` exercises real child processes: separate streams,
input, Unicode/quoting, literal metacharacters, environment selection, timeout,
descendant cleanup, output limits, simultaneous pipe traffic, early input close,
parent exit status and validation failures. This is local Windows evidence, not
a cross-platform certification or complete migration.

Use the [language contract](language-contract.md) and typed JSON APIs for the
host implementation. Watch, cache hashing/atomic replacement, daemon transport,
standalone program executables remain separate CLI infrastructure work.

## Filesystem behavior

Filesystem operations now use UTF-8 paths on Windows, return UTF-8 directory
entry names and reject embedded NUL before opening/removing a truncated path.
The preexisting `..` restriction and read/size failure conventions remain.

The following operations are general CLI capabilities, independent of any
project, build system or target language:

| Operation | Contract |
| --- | --- |
| `Fs.ReadText(path, maxBytes)` | Return raw text bytes; raise an error for missing/nonregular/unreadable files or exceeded budget. Budget: 1 through 64 MiB. Empty files return an empty string. No newline conversion. |
| `Fs.ReadBytes(path, maxBytes)` | Return `array<number>` containing integer bytes 0 through 255. Budget: 1 through 4 MiB. Embedded NUL and invalid UTF-8 bytes are preserved. |
| `Fs.WriteAtomic(path, text)` | Replace one destination using a flushed exclusive temporary file in the same directory; maximum 64 MiB. Return void or raise an error. |
| `Fs.WriteBytesAtomic(path, bytes)` | Same replacement contract for at most 4 MiB of integer bytes 0 through 255. Validate all bytes before touching the destination. |
| `Fs.Stat(path)` | Return `Fs.FileInfo`: exists, isDirectory, isFile, size in bytes and modifiedUnixMs. Missing paths have false flags and zero numeric fields. |

Budgets count bytes, not Unicode characters. These bounded reads distinguish
failure from valid empty content, unlike the legacy `Fs.read` convention. Byte
arrays use more memory than their file payload; choose a workload-appropriate
limit. Metadata follows links. Timestamps are Unix milliseconds, with precision
limited by the underlying filesystem. Matching size and time do not prove equal
contents and are not enough for a correct content cache.

Atomic writes require an existing destination directory. Readers see complete
old or new content on a filesystem supporting atomic same-directory replacement;
Windows readers must allow delete sharing. Brief sharing/access conflicts are
retried for at most approximately 200 ms before a catchable failure. Failed writes
before replacement preserve the old destination and remove their temporary file.
The call does not preserve old permissions/ACLs and does not guarantee directory
metadata durability after a power loss. POSIX temporary/replacement files use
private permissions. No implicit directory creation or multi-file transaction
is performed. An atomic replacement of a hard-link name leaves the other link's
contents unchanged. Applications must still enforce their own input/output rules.

Executable examples: `examples/cli/copy_file.clp` and `examples/cli/file_info.clp`.
The real-process tests in `tests/runtime/cli_files.py` exercise bounded reads,
every byte value, replacement failure, concurrent readers, hard links, cloud
directory resolution, metadata, executable discovery and those CLI programs.
None simulates an application-specific compiler protocol.
