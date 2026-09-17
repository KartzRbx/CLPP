local Operators = {}

--[=[
	@class Operators
	CL++ does **not** use `->`. Four accessors cover Roblox, plus janitor `~>` and C-like arithmetic.

	| Operator | Meaning | Luau |
	| --- | --- | --- |
	| `::` | method / scope | `:` on a call, `.` otherwise |
	| `:` | table / dictionary key | `.` |
	| `.` | Instance or value property | `.` |
	| `.:` | string concatenation | `..` |
	| `~>` | Connect / Once with Janitor | `janitor:Add(..., "Disconnect")` |

	`+` only adds numbers. `!=` `&&` `||` `!` emit `~=` `and` `or` `not`. `++` / `--` emit `+= 1` / `-= 1`.

	Full pages: [Language reference — Operators](/CLPP/docs/reference).
]=]

--[=[
	Method call and nested name.

	**Syntax:** `obj::Method(args);` · `obj::Child` (no call) · `Class::Method` (definition)

	**Emit:** `obj:Method(args)` / `obj.Child`

	```clpp
	player::FindFirstChild("x");
	```

	Datatype / `task::wait` emit `.` (not Instance methods).

	@function ::
	@within Operators
	@tag operator
]=]

--[=[
	Dictionary / module table key. Not a method call.

	**Syntax:** `Table:Key`

	**Emit:** `Table.Key`

	```clpp
	DataService:Server::WaitFor(p);
	post(stats:Coins);
	```

	Range-for uses `in` (or `:`) in a different position: `for (T x in list)`.

	@function :
	@within Operators
	@tag operator
]=]

--[=[
	Instance or value property.

	**Syntax:** `instance.Property`

	**Emit:** `instance.Property`

	```clpp
	player.Name = "Kartz";
	```

	@function .
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
