local Operators = {}

--[=[
	@class Operators
	CL++ does **not** use `->`. `.` is the default (property and instance method). `:` types and protects calls. `::` is static / manual Connect. `~>` is Janitor. `[]` indexes arrays, maps, and dynamic child names — never a C++ capture list. Callbacks are `func (params) { }`.

	| Operator | Meaning | Luau |
	| --- | --- | --- |
	| `.` | property / instance method | `.` / `:` on a call |
	| `:` | type annotation / protected call | `pcall` on a call |
	| `::` | static scope / manual Connect | `.` / `:` |
	| `[]` | array / map / dynamic child index | `[key]` |
	| `.:` | string concatenation | `..` |
	| `~>` | Connect / Once with Janitor | `janitor:Add(..., "Disconnect")` |

	`+` only adds numbers. `!=` `&&` `||` `!` emit `~=` `and` `or` `not`. `++` / `--` emit `+= 1` / `-= 1`.

	Full pages: [Language reference — Operators](/CLPP/docs/reference).
]=]

--[=[
	Property or instance method. The default accessor.

	**Syntax:** `obj.Prop` · `obj.Method(args)`

	**Emit:** `obj.Prop` / `obj:Method(args)`

	```clpp
	player.Name = "Kartz";
	player.Kick();
	workspace.FindFirstChild("x");
	```

	@function .
	@within Operators
	@tag operator
]=]

--[=[
	Type annotation, or a protected (Safe Mode) call.

	**Syntax:** `age: int = 10` · `player:Kick()`

	**Emit:** `local age: number = 10` / `pcall` of the method; `nil` on error.

	```clpp
	age: int = 10;
	auto child = workspace:FindFirstChild("x");
	```

	@function :
	@within Operators
	@tag operator
]=]

--[=[
	Static scope, class method definition, or a Connect you Disconnect yourself.

	**Syntax:** `task::wait(1)` · `Class::Method` · `players.PlayerAdded::Connect(fn)`

	**Emit:** `task.wait(1)` / `function Class:Method` / `signal:Connect(fn)` (no Janitor)

	```clpp
	players.PlayerAdded::Connect(fn);
	task::wait(1);
	```

	Datatype / `task::wait` emit `.`. Prefer `~>` when a janitor should own the connection.

	@function ::
	@within Operators
	@tag operator
]=]

--[=[
	String concatenation. Never `+` or Lua `..`.

	**Syntax:** `a .: b .: c`

	**Emit:** `a .. b` (left-associative)

	```clpp
	return player.Name .: "_LeaderstatsJanitor";
	```

	@function .:
	@within Operators
	@tag operator
]=]

--[=[
	Subscribe and give the connection to Janitor. `~>Connect` and `~>Once` only.

	**Syntax:** `signal~>Connect(fn);` · `signal~>Once(fn);`

	**Emit:** `janitor:Add(signal:Connect(fn), "Disconnect")`

	Looks up `janitor` local, `self.janitor`, or synthetic `__janitor`.

	@param fn func -- Listener
	@function ~>
	@within Operators
	@tag operator
]=]

--[=[
	Index an array, map, or Roblox child by dynamic name. Not a lambda capture.

	**Syntax:** `items[0]` · `data["Coins"]` · `workspace["PlayerCharacter"]`

	**Emit:** `items[0]` / `data["Coins"]` / `workspace["PlayerCharacter"]`

	```clpp
	string first = items[0];
	playerData["Coins"] = 600;
	Instance* root = character["HumanoidRootPart"];
	```

	@function []
	@within Operators
	@tag operator
]=]

--[=[
	Numeric arithmetic. `+` does not concatenate. `*` is never pointer deref.

	**Syntax:** `a + b - c * d / e`

	**Emit:** same operators

	@function +
	@within Operators
	@tag operator
]=]

--[=[
	`!=` emits `~=`. The others stay the same.

	**Syntax:** `a != b` · `a == b`

	**Emit:** `~=` / `==` / `<` / `>` / `<=` / `>=`

	@function !=
	@within Operators
	@tag operator
]=]

--[=[
	Boolean logic. No ternary `? :`.

	**Syntax:** `a && b || !c`

	**Emit:** `and` / `or` / `not`

	@function &&
	@within Operators
	@tag operator
]=]

--[=[
	Assignment and compound assignment. Observable assignment writes `.Value`.

	**Syntax:** `name = value;` · `name += 1;`

	**Emit:** `=` `+=` `-=` `*=` `/=`

	@function =
	@within Operators
	@tag operator
]=]

--[=[
	Always emits `+= 1` / `-= 1`. Prefix/postfix value is not preserved.

	**Syntax:** `i++;` · `i--;`

	@function ++
	@within Operators
	@tag operator
]=]

--[=[
	C++ designated initializer → Luau table field.

	**Syntax:** `Type { .Field = value }`

	**Emit:** `{ Field = value }`

	@function .Field
	@within Operators
	@tag operator
]=]

return Operators
