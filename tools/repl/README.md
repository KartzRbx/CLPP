# tools/repl/

CL++ has no interactive VM, so there is no in-process REPL.

To try a snippet:

```bash
clpp compile file.clpp
```

Then run the emitted `.luau` with Studio, lune, or the Luau CLI. `clpp watch` recompiles a directory when sources change.
