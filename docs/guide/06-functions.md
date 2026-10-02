# 6. Functions

Declare a function with `func name(parameters) -> ReturnType { ... }`. The compiler checks argument and return types. Parameters can have defaults, calls can name arguments, and `...` marks a variadic parameter. Functions may call themselves recursively.

```clp
func add(int a, int b = 1) -> int {
  return a + b;
}
post(add(2));
post(add(b: 4, a: 3));
```

`func identity<T>(T value) -> T` introduces a generic type parameter. `where T: Type` constrains a generic parameter. `namespace Name { ... }` groups functions. An expression lambda uses `(x => expression)`; a block lambda uses `func (parameters) { ... }`.

`extern func` declares a function supplied by the embedding host. This is the connection point between a CL++ program and a C++ game or application. Modules export their declared functions; import them with `link` as described in [Modules](10-modules.md).

Functions can return `void` when they produce no value. A function without an explicit return type can have its result inferred where supported by the declaration.

## More call forms

```clp
func sum(... values) {
  let mut total = 0;
  for (let value in values) { total += value; }
  return total;
}
post(sum(1, 2, 3));
```

A variadic parameter gathers extra arguments. A function can return a list and the caller can unpack it with `let (first, second) = result;`. Namespaces group related functions. The compiler checks that a typed function returns a value on every path and warns about unreachable statements after `return`.

Use a named function for reusable behavior, a generic function when the same operation genuinely works on multiple types, and a local lambda for a short expression at its call site. Lambdas are currently called where written rather than stored as general function values. Use `extern func` only when the host application provides the implementation; it is an interface to native code, not a way to skip an implementation.
