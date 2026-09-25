---
title: Types and values
---

# Types and values

CL++ is statically typed in the source **and** in the Luau it emits. Full rules: [Type system](architecture/TYPE_SYSTEM).

## Primitives

```clpp
int coins = 100;
float speed = 16.5;
string name = "Kartz";
string title = 'Player';
string line = `hello {name}`;
bool isActive = true;
Player playerRef = null;
```

| Write this | Emit this |
| --- | --- |
| `int` `float` `double` | `number` |
| `bool` | `boolean` |
| `string` | `string` |
| `void` | no return annotation |
| `func` | `(...any) -> any` (**lossy** — prefer typed params on named fns) |
| `auto` | inferred from `new`, `GetService`, datatype ctor |
| `null` | `nil` |
| `optional<T>` | `T?` |

Always initialize: `int coins;` emits `nil`. Prefer `int coins = 0;`.

## Aliases, unions, intersections

```clpp
using Coins = int;
type UserId = int;
type Result = string | int;
type Combo = Player & Instance;
type Box<T> = array<T>;
```

`optional<T>` is `T | null`. After `guard (value != null) else { return; }` (or `if (value)`), the checker treats `value` as `T`.

## Enums

```clpp
enum class TradeState { Open, Locked, Complete };
```

`match` / `switch` on enums report missing variants (**CLPP0501**).

## Checked generics

```clpp
interface Drawable { void render(); };

template <typename T : Drawable>
void draw(T item) {
    item.render();
}
```

Unbounded `T` cannot access members. Call sites must satisfy the bound (**CLPP0901**). See [RFC 0010](https://github.com/KartzRbx/CLPP/blob/main/rfc/0010-checked-generics.md).

## Instances are class names

`Player player` is an Instance of class Player. There is no address, no `delete`, and no `->`. Properties and instance methods use `.`. Protected calls use `:`. Static names use `::`.

```clpp
player.Name = "Kartz";
player.FindFirstChild("leaderstats");
```

## Datatypes

```clpp
part.Size = Vector3(8, 1, 8);
part.Material = Enum.Material.Plastic;
```

These are **values**. Do not write `new Vector3`.

## Casts

`static_cast<Folder>(existing)` and `existing as Folder` emit `existing`. Luau does not check `ClassName` at the cast. `FindFirstChild<Folder>("leaderstats")` types the result as `optional<Folder>`.

## `const`

`const` / `static constexpr` become Luau `const` (binding immutability — not deep freeze).

## Modules

Bring types from other files with [`link`](modules).

See [Type table](spec/types). Next: [Operators](operators).
