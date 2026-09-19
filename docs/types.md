---
title: Types and values
---

# Types and values

CL++ is statically typed in the source **and** in the Luau it emits.

## Primitives

```clpp
int coins = 100;
float speed = 16.5;
string name = "Kartz";
string title = 'Player';
string line = `hello {name}`;
bool isActive = true;
func callback = func () {};
Player playerRef = null;
```

| Write this | Emit this |
| --- | --- |
| `int` `float` `double` | `number` |
| `bool` | `boolean` |
| `string` | `string` |
| `void` | no return annotation |
| `func` | `(...any) -> any` |
| `auto` | inferred from `new`, `GetService`, datatype ctor |
| `null` | `nil` |
| `optional<T>` | `T?` |

Always initialize: `int coins;` emits `nil`. Prefer `int coins = 0;`.

## Instances are class names

`Player player` is an Instance of class Player. There is no address, no `delete`, and no `->`. Properties and instance methods use `.`. Protected calls use `:`. Static names use `::`.

[`match`](guard-match) arms use the same class name: `Part p =>`.

```clpp
player.Name = "Kartz";
player.FindFirstChild("leaderstats");
```

## Datatypes

```clpp
part.Size = Vector3(8, 1, 8);
part.CFrame = CFrame.lookAt(from, look);
part.Material = Enum.Material.Plastic;
```

These are **values**. Do not write `new Vector3`.

## Casts

`static_cast<Folder>(existing)` emits `existing`. Luau does not check `ClassName` at the cast.

## `const`

`const` / `static constexpr` become Luau `const`.

See the generated [Types API](/api/Types). Next: [Operators](operators).
