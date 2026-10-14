# 11. Errors and diagnostics

Lexical errors concern characters and tokens; syntax errors concern statement structure; name errors concern unresolved declarations; type errors concern incompatible values. The compiler reports these before execution with a source file, line, and column. A runtime failure happens while the program runs, for example an invalid collection index or division by zero.

## Catching runtime failures

`try { ... } catch (name) { ... }` runs the protected block. A runtime error jumps to `catch`, whose binding contains the error message. Compilation errors cannot be caught this way.

```clp
try {
  post(1 / 0);
} catch (error) {
  post("The operation failed");
}
post("continuing");
```
```output
The operation failed
continuing
```

The binding is optional: `catch { ... }` handles a failure without naming its message. Protect the operation you can recover from so unrelated failures remain visible.

## Protected expressions

`pcall(expression)` evaluates an expression under protection and reports whether evaluation succeeded. It does not provide a `Result<T, E>` payload.

```clp
post(pcall(1 / 0));
post(pcall(8 / 2));
```
```output
0
1
```

The current executable prints the protected-call status numerically: `0` for failure and `1` for success. This output is not evidence for a target boolean-result contract.

## Output and intentional failures

`post` writes normal output. `warn` reports a nonfatal warning. `report` raises a failure; continuing after it would be invalid. An uncaught compilation or execution failure makes the command line exit with a nonzero status.

```clp
try { report("missing player"); }
catch (error) { post("Player unavailable"); }
```
```output
Player unavailable
```

For an expected domain outcome, return explicitly modeled data. The target success/error wrapper contract is not settled; see [types and alternatives](08-enums-variants-option-result.md).

## Editor and diagnostic reference

The language server underlines errors while editing and associates imported-module failures with the `link` statement. See the [toolchain reference](../reference/toolchain.md) for compiler commands and diagnostic output. Fix the first relevant source error, then compile again; later messages can follow from that initial error.

The compiler accepts `pragma strict;` and `pragma nostrict;` at file start. Older directive markers and unknown modes are rejected. See [comments and directives](../reference/comments.md) and [implementation status](../reference/status.md).

## Intended diagnostic behavior

The author expects one message per root cause, with source position and a useful correction. A broken declaration must not generate a second error for every later use of that name. This recovery rule is a compiler requirement, not a passing claim about the existing checker.

| Invalid target program | Expected explanation |
| --- | --- |
| Assigning to `const` | Identify the immutable declaration and the assignment |
| Calling a callback with the wrong signature | Show required parameters/result and received signature |
| Missing an enum case in exhaustive `match` | Name the missing case |
| Reading a missing optional without narrowing | Require a presence check or guard |
| Using an inaccessible module type | Point to its declaration and visibility |

Exact diagnostic identifiers from the old design are not release guarantees. The compiler revision must establish and test the final code, severity, position, and correction for each case.
