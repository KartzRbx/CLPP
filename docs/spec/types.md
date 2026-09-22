# Types

CL++ is statically typed in what you write and in the Luau it emits. Narrative guide: [Types and values](../types). Formal surface: [TYPE_SYSTEM](../architecture/TYPE_SYSTEM).

## Primitives and containers

| CL++ | Luau | Use |
| --- | --- | --- |
| `int` / `float` / `double` | `number` | counts, damage, alpha |
| `bool` | `boolean` | flags |
| `string` | `string` | names, paths |
| `void` | no return annotation | procedures |
| `func` | `(...any) -> any` | loose callback escape hatch |
| `auto` | inferred | `new`, `GetService`, datatype ctor |
| `Player` / `Folder` | same | Instance (class name) |
| `array<T>` / `vector<T>` / `span<T>` | `{T}` | arrays |
| `dictionary<K, V>` | `{ [K]: V }` | maps |
| `optional<T>` | `T?` | missing value (= `T \| null`) |
| `A \| B` / `A & B` | Luau union / intersection | aliases |
| `type` / `using` | type alias | including `type Box<T> = …` |
| `enum` / `enum class` | string/number enums | exhaustiveness |
| `template <typename T : B>` | erased / name-only | checked generics |
| `null` | `nil` | absence |
| `Vector3` `CFrame` … | same | datatypes |
| `Enum.Material.Plastic` | same | Roblox enums |

## Assignability (summary)

- `int` widens to `float`.
- `optional<T>` does **not** assign to plain `T` (CLPP0201) until narrowed.
- Generic `T : Bound` accepts values that nominally or structurally satisfy `Bound` (CLPP0901).

## Casts

`static_cast` / `as` emit the value with no runtime `ClassName` check. Document that when writing host tools.

There is no `delete`, no address-of, and no `->`.
