# 16. Performance

CL++ compiles source to verified bytecode and executes it on a register virtual machine. The compiler sizes each function's register frame to its actual needs, and the VM reuses frames. Numeric values take a specialized fast path; indexed reads avoid copying an entire collection. The compiler emits typed integer operations when both operands are `number`.

## Measured results

These historical repository benchmarks are medians of seven release-build runs on a 2.1 GHz Xeon with two cores. Times include compilation and startup; they are workload-specific, not a general speed guarantee.

| Program | Earlier measured revision | Later measured revision | CPython 3.11 |
| --- | ---: | ---: | ---: |
| `fib(27)` | 1.605 s | 0.095 s | 0.031 s |
| 3 million arithmetic loop iterations | 0.379 s | 0.180 s | 0.264 s |
| 300,000 virtual method calls | 0.595 s | 0.109 s | 0.035 s |
| 200,000 `Vector3` physics steps | 0.353 s | 0.050 s | 0.087 s |
| 100,000 templates and concatenations | 0.034 s | 0.023 s | 0.024 s |

To reproduce the measurement, run `python3 benchmarks/run.py build/release/src/clpp --runs 7 --markdown` after building the release preset.

Bytecode passes structural and stack verification before execution. Recursion has a configurable limit, invalid indices raise errors, and the `asan` preset enables AddressSanitizer for the test suite. These checks describe the existing VM. Its old data-copy behavior differs from the target `def` reference semantics and must not be used as a language specification.

## Read the results correctly

The arithmetic loop, vector workload, and string workload were faster than the compared CPython 3.11 runs on this machine; recursive and virtual calls were slower. These measurements include compiler startup, use particular program sizes, and do not predict every game or tool. Benchmark your own workload in release mode before choosing an optimization. Prefer a clear algorithm first, then profile. Ownership and explicit-copy optimizations must be remeasured after the compiler is aligned with the target object model.
