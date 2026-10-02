# Backend capabilities

CLIR v1 may contain host opcodes. A backend must either lower them or reject the chunk with `check_backend`.

| Opcode / capability | VM | Luau text backend |
|---------------------|----|-------------------|
| Arithmetic, locals, jumps, calls, `post` | yes | subset via `clir_to_luau` (literals, locals, `if` / `while`, calls written as calls, `return`, `post` as `print`) |
| `FileSize`, `Env` | yes | rejected |
| `HttpHost` (host parse, not a network client) | yes | rejected |
| Atomics, mutex, `SpawnThread`, `Join` | yes | rejected |
| `Actor` sandbox | yes | rejected |
| Field and index assignment (`SetField`, `SetIndex`, `Over`), list mutation (`ListPush` … `ListRemove`), `len`, collection `for … in` (`IterLen`, `IterAt`), method self write-back (`MarkSelf`, `SelfBack`) | yes | not in the text subset |
| Tasks, `await`, `spawn`, `parallel` | yes | not in the text subset; opcode is not rejected by `check_backend` because a future lowering could exist, but `clir_to_luau` reports an unsupported statement if the AST uses them |

The VM is the reference backend. The Luau text backend is kept as an optional exporter for a small subset; CL++ semantics are defined by the VM, not by Luau.

`check_backend(Backend::Vm, chunk)` accepts every decoded opcode. `check_backend(Backend::Luau, chunk)` rejects the host rows above. The Luau emitter walks the analyzed AST, not a decompiler, and does not run Luau. Cluaupp is not vendored here.
