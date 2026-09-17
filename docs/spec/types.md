# Types

CL++ is statically typed in what you write and in the Luau it emits.

## Primitives

| CL++ | Luau | Use |
| --- | --- | --- |
| `int` / `float` / `double` | `number` | counts, damage, alpha |
| `bool` | `boolean` | flags |
| `string` | `string` | names, paths (not `std::string`) |
| `void` | no return annotation | procedures |
| `func` | `(...any) -> any` | callbacks / lambdas |
| `auto` / `auto*` | inferred | `new`, `GetService`, datatype ctor |
| `Player*` / `Folder*` | `Player` / `Folder` | Instance, not an address |
| `array<T>` / `LuaArray<T>` / `vector<T>` / `span<T>` | `{T}` | arrays |
| `dictionary<K, V>` | `{ [K]: V }` | tables / maps |
| `optional<T>` | `T?` | missing value |
| `const` / `static constexpr` | Luau `const` | immutable |
| `null` | `nil` | absence |
| `Vector3` `CFrame` `UDim2` `Color3` | same names | datatypes (copy) |
| `Enum.Material.Plastic` | `Enum.Material.Plastic` | enums |

Examples:

```clpp
int coins = 100;
float speed = 16.5;
string name = "Kartz";
bool isActive = true;
func callback = []() {};
auto dynamicVal = DataService:Server;
Player* playerRef = null;
```

Always initialize: `int coins = 0;` — `int coins;` emits `nil`.

There is no `delete`, `*part`, `&part`, `int&`, or `->`. Numbers copy; Instances mutate through `::` (method) and `.` (property).

## Pointers are Instances

`Player*` means “an Instance of class Player”. Properties use `.`. Methods use `::`.

```clpp
player.Name = "Kartz";
player::FindFirstChild("leaderstats");
```

```luau
player.Name = "Kartz"
player:FindFirstChild("leaderstats")
```

There is no pointer arithmetic and no `std::unique_ptr`. Lifetime is Roblox's: `Destroy` or Janitor.

## `auto`

Inferred when the value is `new Class(...)`, `GetService<T>()`, or a datatype constructor. Prefer an explicit type on parameters and `struct` fields.

## Casts

`static_cast<T>(x)`, `const_cast`, `reinterpret_cast`, and `dynamic_cast` emit the argument. `(void)x;` disappears (silences unused in clangd). Luau has no casts — they do not check `ClassName`.
