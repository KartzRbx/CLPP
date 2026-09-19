---
title: Syntax specification
---

# CL++ syntax → Luau

CL++ is the **language**: syntax, semantics, and OOP. Connecting to the Roblox API (generated headers, runtime) is **Cluaupp**'s job. The `clpp` compiler emits [Luau](https://luau.org/getting-started).

Generated Luau uses `local`, `const`, and `const function`. Injected `require` / `GetService` are `const`. `--!strict` / `--!nonstrict` / `--!native` / `--!optimize N` come from `#pragma`.

Extensions and tags: [Files](files.md). Types: [Types](types.md). Emit: [Mapping](emit.md).

## Functions

```clpp
void CreateLeaderstats(Player* player) {
    return;
}

const int DoubleCoins(int coins) {
    return coins;
}
```

```luau
const function CreateLeaderstats(player: Player)
	return
end

const function DoubleCoins(coins: number): number
	return coins
end
```

- Only functions with a body emit from a `.clp` / `.clpp`. Prototypes in a `.clh` become `export type` fields.
- No overloading — one name, one emit. No default arguments.
- `void init()` at the end of Script / LocalScript. There is no `int main()`.

## Scopes

- File-level declarations become file `local` / `const`.
- Function declarations last from that line to the end of the block. Nested `{ }` may shadow.
- In `Class::Method`, `@this` / `this` is `self`. `@field` and bare fields become `self.field`. Calls to other methods become `self:Method(...)`. Parameters and locals shadow fields.
- Globals stay bare: `post`, `warn`, `report`, `game`, `workspace`, lib types, Instances, datatypes.
- Instances are not RAII. Leaving a block **does not** `Destroy` — use Janitor.

## Control flow

```clpp
if (coins > 50) {
    post("Enough balance!");
} else if (coins == 0) {
    warn("No coins!");
} else {
    report("Balance sync error.");
}

while (true) {
    post("tick");
}

for (int i = 0; i < 10; i++) {
    post("Count: " .: i);
}

for (Player* player in players.GetPlayers()) {
    post("Player connected: " .: player.Name);
}

switch (action) {
case "buy":
case "purchase":
    Grant(player);
    break;
case "sell":
    if (amount <= 0) {
        break;
    }
    Take(player);
    break;
default:
    warn("unknown");
    break;
}
```

```luau
if coins > 50 then
	print("Enough balance!")
elseif coins == 0 then
	warn("No coins!")
else
	error("Balance sync error.")
end

while true do
	print("tick")
end

local i = 0
while i < 10 do
	print("Count: " .. i)
	i += 1
end

for _, player in players:GetPlayers() do
	print("Player connected: " .. player.Name)
end
```

`switch` evaluates the discriminant **once**, then becomes `if` / `elseif` / `else` inside `repeat … until true` so `break` still leaves the switch. Stacked `case`s share the body. No C-style fall-through.

`else if` emits Luau `elseif`. Range-for: `for (Type name : list)`. C loop: `for (int i = 0; i < n; i++)`. Unsupported: `continue`, `do/while`, ternary, `->`.

## Expressions and operators

| CL++ | Luau |
| --- | --- |
| `null` | `nil` |
| `true` / `false` | `true` / `false` |
| `!=` | `~=` |
| `&&` | `and` |
| `\|\|` | `or` |
| `!` | `not` |
| `==` `+` `-` `*` `/` `<` `>` `<=` `>=` | same (`*` is multiply; `+` only adds numbers) |
| `"text"` | `"text"` |
| `=` `+=` `-=` `*=` `/=` | same |
| `++` / `--` | `+= 1` / `-= 1` |
| `obj.Prop` | `obj.Prop` |
| `obj.Method(a)` | `obj:Method(a)` |
| `obj:Method(a)` | `pcall` of `obj:Method(a)` |
| `DataService.Server` | `DataService.Server` |
| `a .: b` | `a .. b` |
| `fn(a)` | `fn(a)` |
| `Type { .Field = value }` | `{ Field = value }` |
| unary `-` | `-` |

Binary operators associate left-to-right — use parentheses when mixing.

Designated initializers become Luau tables:

```clpp
DataService.Server.Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true,
});
```

```luau
DataService.Server:Init({
	Template = playerData,
	StoreName = "PlayerData",
	UseMock = true,
})
```

## Strings

Concatenation is the `.:` operator. It emits Luau `..`. **Do not** write `..` or use `+` to join text.

- `a .: b .: c` associates left-to-right: `(a .. b) .. c`
- `string_concat(a, b, c)` remains valid: zero args → `""`, one arg → the value, several → `(a .. b .. c)`
- `to_string(x)` / `to_number(x)` / `to_bool(x)` convert. Do not write Luau `tostring` / `tonumber`.

```clpp
return player.Name .: "_LeaderstatsJanitor";
return player.Name .: "_" .: "LeaderstatsJanitor";
return string_concat(player.Name, "_", "LeaderstatsJanitor");
```

```luau
return player.Name .. "_LeaderstatsJanitor"
return (player.Name .. "_") .. "LeaderstatsJanitor"
return (player.Name .. "_" .. "LeaderstatsJanitor")
```

## post / warn / report

| CL++ | Luau |
| --- | --- |
| `post(...)` | `print(...)` |
| `warn(...)` | `warn(...)` |
| `report(...)` | `error(...)` |
| `to_string(x)` | `tostring(x)` |
| `to_number(x)` | `tonumber(x)` |
| `to_bool(x)` | `not not (x)` |

`cout << … << endl` remains valid (C++ legacy): each `<<` is another argument, `endl` ends the line.

```clpp
post("ok");
warn("careful");
report("failed");
```

```luau
print("ok")
warn("careful")
error("failed")
```

## `new` and services

```clpp
auto* coins = new IntValue(leaderstats);
auto* players = GetService<Players>();
auto* janitor = new Janitor();
part.Size = Vector3(8, 1, 8);
part.CFrame = CFrame.lookAt(from, look);
part.Material = Enum.Material.Plastic;
```

```luau
local coins: IntValue = Instance.new("IntValue")
coins.Parent = leaderstats
local players: Players = game:GetService("Players")
local janitor = Janitor.new()
part.Size = Vector3.new(8, 1, 8)
part.CFrame = CFrame.lookAt(from, look)
part.Material = Enum.Material.Plastic
```

The first argument of `new Class(parent)` becomes `.Parent` on Instance classes. Libs use `Janitor.new()`. Datatypes use `Vector3(...)`, not `new`.

## Callbacks

Anonymous callbacks are `func (params) { }`. There is no C++ capture list. `func` is also the type. Luau closures capture the environment; keep Instances alive with Janitor.

```clpp
func onCoinsChanged = func (int newValue) {
    post("New value: " .: newValue);
};

players.PlayerAdded::Connect(func (Player* playerEntered) {
    post("New player: " .: playerEntered.Name);
});

game.BindToClose(func () {
    janitor.Cleanup();
});
```

Passing a method by name from inside `Class::` binds `self`: `function(...) self:OnPlayer(...) end`.

## Collections

```clpp
array<string> names = {"Kartz", "Player1"};

dictionary<string, int> stats = {
    {"Coins", 100},
    {"Gems", 50}
};
```

```luau
local names: {string} = { "Kartz", "Player1" }
local stats: { [string]: number } = { Coins = 100, Gems = 50 }
```

## OOP: structs, methods, singletons

The type lives in a `struct` (or `class`) in the sibling `.clh`. Methods are `Class::Method` in the `.clp` / `.clpp`. `void init()` starts the script.

- `static constexpr` becomes `const`.
- Fields are `self.field`. `public:` / `private:` are ignored.
- Nested structs with defaults become nested tables (DataService Templates). `array<T>` / `LuaArray<T>` fields stay on the table.
- `Class::` emits `function Class:Method(...)`.
- Construct **one** service in `init()` (`LeaderstatsServer leaderstatsServer;`) and use it in lambdas. That is the game singleton.
- Lib singletons: `DataService.Server` / `DataService.Client`.
- Field-only structs in the header emit `const function Name()` with defaults.
- Untagged files with only `Class::` `return` the table (ModuleScript).

Stems must match: `LeaderstatsServer.clh` next to `LeaderstatsServer.server.clpp`.

## `.` vs `:` vs `::` vs `~>`

CL++ **does not use `->`**. `.` is the default (properties and instance methods). `:` types a name or wraps a call in `pcall`. `::` is static scope and manual Connect. `~>` gives the connection to Janitor. Concatenation is `.:`.

| CL++ | Luau | When |
| --- | --- | --- |
| `player.Name` | `player.Name` | property |
| `player.Kick()` | `player:Kick()` | instance method |
| `player:Kick()` | `pcall` of `player:Kick()` | protected call |
| `age: int = 10` | `local age: number = 10` | type |
| `players.PlayerAdded::Connect(fn)` | `players.PlayerAdded:Connect(fn)` | manual Connect |
| `players.PlayerAdded~>Connect(fn)` | `janitor:Add(..., "Disconnect")` | Janitor Connect |
| `task::wait(1)` | `task.wait(1)` | static / library |
| `DataService.Server.WaitFor(p)` | `DataService.Server:WaitFor(p)` | table + method |
| `janitor.Add(conn)` | `janitor:Add(conn)` | instance method |
| `a .: b` | `a .. b` | concatenation |
| `for (T x in list)` | `for _, x in list do` | range-for (`:` still works) |
| `signal.Fire(...)` | `signal:Fire(...)` | emit / send |
| `obj.GetPropertyChangedSignal("Name")` | `obj:GetPropertyChangedSignal("Name")` | property change signal |

## observable

`observable T name = value` becomes a ValueBase. Reads/writes use `.Value`. `OnChange` listens to `Changed`.

```clpp
observable int coins = 100;

coins.OnChange(func (int newValue) {
    post("Coins changed to: " .: newValue);
});

coins = 50;
post(coins);
```

```luau
local coins: IntValue = Instance.new("IntValue")
coins.Value = 100
coins.Changed:Connect(function(newValue: number)
	print("Coins changed to: " .. newValue)
end)
coins.Value = 50
print(coins.Value)
```

`int` → `IntValue`, `float`/`double` → `NumberValue`, `string` → `StringValue`, `bool` → `BoolValue`, everything else → `ObjectValue`.

## guard

```clpp
guard (player != null) else {
    warn("Invalid player");
    return;
}
post(player.Name);
```

```luau
if not (player ~= nil) then
	warn("Invalid player")
	return
end
print(player.Name)
```

## Auto-cleanup `~>`

`signal~>Connect(fn)` and `signal~>Once(fn)` register the connection on the scope Janitor (`janitor` local, `self.janitor`, or a synthetic `__janitor`). `Once` disconnects after the first emission. In `void init()`, the synthetic `__janitor` also gets `game:BindToClose`.

```clpp
players.PlayerAdded~>Connect(func (Player* player) {
    post("Connected and managed automatically!");
});

players.PlayerAdded~>Once(func (Player* player) {
    post("First player only");
});
```

```luau
janitor:Add(players.PlayerAdded:Connect(function(player: Player)
	print("Connected and managed automatically!")
end), "Disconnect")
janitor:Add(players.PlayerAdded:Once(function(player: Player)
	print("First player only")
end), "Disconnect")
```

## signal

```clpp
signal<Player*, int> OnCoinsUpdated;

OnCoinsUpdated~>Connect(func (Player* player, int newAmount) {
    post(player.Name .: ": " .: newAmount);
});

OnCoinsUpdated~>Once(func (Player* player, int newAmount) {
    post("first: " .: newAmount);
});

OnCoinsUpdated.Fire(player, 500);

part.GetPropertyChangedSignal("Transparency")~>Connect(func () {
    post(part.Transparency);
});
```

```luau
local OnCoinsUpdated = __signal()
janitor:Add(OnCoinsUpdated:Connect(function(player: Player, newAmount: number)
	print(player.Name .. ": " .. newAmount)
end), "Disconnect")
janitor:Add(OnCoinsUpdated:Once(function(player: Player, newAmount: number)
	print("first: " .. newAmount)
end), "Disconnect")
OnCoinsUpdated:Fire(player, 500)
part:GetPropertyChangedSignal("Transparency"):Connect(function()
	print(part.Transparency)
end)
```

| CL++ | Meaning |
| --- | --- |
| `signal<T...> name;` | typed signal (`BindableEvent` under `__signal()`) |
| `name.Fire(...)` | **send** the signal |
| `name~>Connect(fn)` | listen until disconnected (Janitor) |
| `name~>Once(fn)` | listen **once**, then disconnect |
| `name.Wait()` | yield until the next fire |
| assign an `observable` | writes `.Value` and fires `Changed` |
| `obj.GetPropertyChangedSignal("X")` | Instance property change signal |

## async / await

```clpp
async Data* FetchData(Player* player) {
    Data* data = await DataService.Server.WaitFor(player);
    return data;
}
```

`await` calls `__await`: if the value has `:expect()` (Promise), wait; otherwise return the value (already yielded).

## Destructuring, spawn, parallel, match, targets

```clpp
auto [success, result] = pcall(func () {
    return DataStore.GetAsync("PlayerData");
});

spawn {
    task::wait(2);
    post("Delay finished!");
};

parallel {
    ComputeComplexPhysics();
};

match (instance) {
    Part p => p.BrickColor = BrickColor::Red(),
    Model m => m.PrimaryPart.BrickColor = BrickColor::Blue(),
    _ => warn("Instance not supported")
};

[[server]]
void SaveData(Player* player) {}

[[client]]
void UpdateUI() {}
```

| CL++ | Luau |
| --- | --- |
| `auto [a, b] = fn();` | `local a, b = fn()` |
| `spawn { ... };` | `task.spawn(function() ... end)` |
| `parallel { ... };` | `task.desynchronize()` / `task.synchronize()` |
| `match (x) { Type t => ... }` | `if x:IsA("Type")` or `typeof` |
| `[[server]]` / `[[client]]` | omitted on the opposite file; in a module, `RunService:IsServer()` |

## Unsupported

Custom C++ classes as metatables; templates beyond `GetService<T>` / `static_cast<T>` / `array<T>` / `dictionary<K,V>` / `LuaArray<T>` / `string_concat`; pointer arithmetic; `->`; `std::` (except mapped aliases); overloading; macros (except `#pragma once` / `strict` / `nostrict` / `native` / `optimize`); JSX; `continue`; ternary; `do/while`; `try/catch`; `goto`; `int&` references; C++ lambda captures (`[x]`, `[&]`).
