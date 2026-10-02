# 9. Collections and text

Use `list(...)` for a growable sequence, `array<T>` for a typed array, and `dictionary<K, V>` for keyed values. Collection indices start at zero. Out-of-range access reports a catchable runtime error. Lists and arrays can be iterated with `for ... in`.

```clp
let mut items = list("sword", "shield");
push(items, "potion");
post(len(items));
post(items[0]);
```

The built-in collection functions include `push`, `pop`, `insert`, `remove`, `find`, and `sort`. Value semantics mean that assigning a collection copies it; use `move` when you want to transfer a large value.

Strings contain UTF-8 text. Indexing and iteration work by character. A slice `text[start .. end]` excludes `end`. `.:` concatenates a value with text, and backtick templates interpolate expressions. The `@clpp.text` module provides additional string operations. `buffer::create(size)` creates a byte buffer for binary data.

## Which collection?

Use a `list` when the number of elements changes and different values are acceptable. Use `array<T>` when every element has a known type and you want the compiler to check member access. Use a `dictionary<K, V>` for lookup by a meaningful key, such as a player ID or configuration name. Use `Vector2/3/4` for coordinates and mathematical operations, not as a general container. Use `buffer` for bytes, not for Unicode text. Prefer a typed struct over a dictionary when the set of fields is fixed and central to the program.

Check the collection length before indexing with untrusted input. Use `for ... in` when you need each element rather than manually tracking an index.
