# Numeric types vs C++ (3.3f)

CL++ `int` and `float` are both Luau `number`.

- `int x = 3.5;` is a type error / warning: Luau **does not truncate**.
- `int / int` emits Luau `/` (float division). Use `div(a, b)` for `//`.
- `pow(a, b)` and `**` emit `^`.
- `size(x)` and `x.size()` emit `#x`.
