# 11. Errors and diagnostics

The compiler reports lexical, syntax, name, and type errors before execution. Diagnostics include a source file, line, and column. Runtime errors, such as division by zero or an invalid index, can be caught with `try` and `catch`.

```clp
try {
  post(1 / 0);
} catch (error) {
  post("The operation failed");
}
```

`post` writes normal output, `warn` writes a warning, and `report` reports a failure. `pcall(expression)` provides a protected call and reports whether it succeeded. The command line exits with a nonzero status for compilation or execution failures. In VS Code, the language server underlines errors as you type and links imported-module errors back to the `link` statement.

Type errors are generally best fixed at the source; `<<!nocheck` is available for exploratory files where type checking should be disabled.

## When to use each form

Use `try`/`catch` around an operation that can fail at runtime and for which your program has a recovery path, such as asking for another file. Use `warn` to report a nonfatal condition, and `report` when continuing would give an invalid result. Use `pcall` for a simple success test when the error detail is not needed. Do not wrap every statement in `try`: a broad catch can hide the exact operation that failed. Do not use `<<!nocheck` as a permanent substitute for correcting type errors.
