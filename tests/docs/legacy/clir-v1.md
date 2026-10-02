# CLIR v1

CLIR v1 is the in-memory compiler IR shared by every backend. It is the stack bytecode already stored in `BytecodeChunk`, identified by `kBytecodeFormatVersion` (`1`). v1 is not a separate on-disk container and not a Luau bytecode format.

`ClirChunk` is a name for `BytecodeChunk`. Addresses and pool indexes are **16-bit**. A chunk whose `code` is larger than `65535` bytes is rejected. Wider addresses belong to a future version, not to v1.

## Layout

| Field | Meaning |
|-------|---------|
| `version` | Must equal `kBytecodeFormatVersion` (1). |
| `code` | Little-endian stack instructions. Jump and entry operands are byte offsets into this buffer and must land on an instruction boundary. |
| `constants` | `Value` pool. `Const`, `GetSelf`, and `SetSelf` index it. |
| `functions` | `arity`, `local_count`, and `entry` byte offset. |
| `vtables` | Function indexes used by `Tag` / virtual calls. |
| `entry` | Byte offset of the top-level script. |
| `local_count` | Locals visible to the top-level region. |
| `source_map` | Optional `(pc, line, column)` records emitted by codegen. Diagnostics prefer the nearest entry at or before the failing instruction. |

There is no required magic number in process memory. The test serializer (`serialize_clir` / `deserialize_clir`) writes the ASCII magic `CLIR`, the version, and the code and function tables so round-trips can be checked. That blob is not a stable distribution format.

## Instructions

Opcodes are the values in `src/core/compiler/opcode.hpp`. Operand widths match `bytecode_format.cpp`:

- Most control, call, local, and constant ops carry one `u16` immediate. This includes the local-slot ops added in the review: `SelfBack`, `ListPush`, `ListPop`, `ListInsert`, `ListRemove`.
- `VCall` carries two `u16` immediates (target and argument count).
- `MakeVector`, `GetField` and `SetField` carry one `u8` extra (component count or field index).
- The full opcode table, with stack effects, is in [opcodes.md](opcodes.md).
- Stack arithmetic, `Post`, `Halt`, `Return`, and similar ops have no immediate.

`verify_bytecode` checks structure: version, size, decode, entries, opcode range (byte `55` is reserved and rejected), constant and function indexes, locals against the region limit, vtable indexes, tag indexes, vector arity `2..4`, `Std`/`Axiom` arity in the high byte of the immediate, and jumps that stay inside the same entry region.

`verify_stack` checks stack depth for the VM lowering. Underflow and conflicting depths at a join are not part of the structural verifier. `lower_to_vm` runs both before it allocates registers.

## Intrinsics

Host operations (`FileSize`, `Env`, `HttpHost`, atomics, mutexes, native threads, `Actor`) are real CLIR opcodes. Whether a backend must implement them is listed in [backend-capabilities.md](backend-capabilities.md). The VM implements the current set. The Luau text backend rejects host opcodes it cannot lower.

## Pipeline

```text
source → analyze_program → AnalysisResult
       → emit_clir → ClirChunk
       → verify_bytecode + verify_stack
       → lower_to_vm  or  clir_to_luau
```

`Compiler::compile` remains the facade: analyze, then emit. Execution still lowers to the register VM.
