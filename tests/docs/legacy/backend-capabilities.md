# VM capabilities

CL++ executes verified bytecode on its own VM. Host operations include file access, environment access, URL host parsing, atomics, mutexes, native threads, and actors. Modules expose these operations through the standard library.

Arithmetic, locals, jumps, calls, output, fields, indexing, collection mutation, iteration, tasks, and asynchronous execution are implemented by the VM. Bytecode verification checks instructions and their operands before execution.

A transpiler written in CL++ may generate another language as application output. That use case does not add an official compiler backend.
