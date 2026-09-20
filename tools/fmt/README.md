# tools/fmt/

The formatter is `clpp fmt` in the compiler (indent only; it does not rewrite syntax).

```bash
clpp fmt path/to/file.clpp          # print to stdout
clpp fmt path/to/file.clpp --write  # replace the file
```

The editor pack’s `textDocument/formatting` calls `clpp api format` with `{ "source" }`.

This folder is a pointer, not a second formatter implementation.
