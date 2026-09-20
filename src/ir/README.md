# src/ir/

There is **no** CL++ intermediate representation in 0.4.

The pipeline is preprocess → parse → analysis → **direct Luau emit**. An IR layer would only exist if emit needed a second lowering step. Until then, do not add `ir/` types that the compiler does not use.

Luau already has bytecode and a VM. CL++ stays source-to-source.
