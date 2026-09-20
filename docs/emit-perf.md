# Emit cost notes (5.2)

- Protected `:` is `pcall`. Prefer `pcall(obj.Kick, obj)` when calling one method in a hot path.
- Long `.:` chains should become backtick interpolation or `table.concat`.
- `clpp emit --no-banner` drops the compiled-by comment.
- `[[native]]` on a function emits `@native`.
- `to_bool(x)` keeps `not not (x)`.
- `#if DEBUG` / `#if RELEASE` drop dead branches to blank lines (line map preserved).
