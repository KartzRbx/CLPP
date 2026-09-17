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
	players::PlayerAdded::Connect(fn);
	player::FindFirstChild("x");
	```

	Datatype / `task::wait` emit `.` (not Instance methods).

	@within Operators
	@tag operator
]=]
function Operators.method_scope() end

--[=[
	Dictionary / module table key. Not a method call.

	**Syntax:** `Table:Key`

	**Emit:** `Table.Key`

	```clpp
	DataService:Server::WaitFor(p);
	post(stats:Coins);
	```

	Range-for uses `in` (or `:`) in a different position: `for (T x in list)`.

	@within Operators
	@tag operator
]=]
function Operators.table_key() end

--[=[
	Instance or value property.

	**Syntax:** `instance.Property`

	**Emit:** `instance.Property`

	```clpp
	player.Name = "Kartz";
	```

	@within Operators
	@tag operator
]=]
function Operators.property() end

--[=[
	String concatenation. Never `+` or Lua `..`.

	**Syntax:** `a .: b .: c`

	**Emit:** `a .. b` (left-associative)

	```clpp
	return player.Name .: "_LeaderstatsJanitor";
	```

	@within Operators
	@tag operator
]=]
function Operators.concat() end

--[=[
	Subscribe and give the connection to Janitor. `~>Connect` and `~>Once` only.

	**Syntax:** `signal~>Connect(fn);` · `signal~>Once(fn);`

	**Emit:** `janitor:Add(signal:Connect(fn), "Disconnect")`

	Looks up `janitor` local, `self.janitor`, or synthetic `__janitor`.

	@param fn func -- Listener
	@within Operators
	@tag operator
]=]
function Operators.janitor_connect(fn) end

--[=[
	Numeric arithmetic. `+` does not concatenate. `*` is never pointer deref.

	**Syntax:** `a + b - c * d / e`

	**Emit:** same operators

	@within Operators
	@tag operator
]=]
function Operators.arithmetic() end

--[=[
	`!=` emits `~=`. The others stay the same.

	**Syntax:** `a != b` · `a == b`

	**Emit:** `~=` / `==` / `<` / `>` / `<=` / `>=`

	@within Operators
	@tag operator
]=]
function Operators.comparison() end

--[=[
	Boolean logic.

	**Syntax:** `a && b || !c`

	**Emit:** `and` / `or` / `not`

	No ternary `? :`.

	@within Operators
	@tag operator
]=]
function Operators.logic() end

--[=[
	Assignment and compound assignment.

	**Syntax:** `name = value;` · `name += 1;`

	**Emit:** `=` `+=` `-=` `*=` `/=`

	Observable assignment writes `.Value`.

	@within Operators
	@tag operator
]=]
function Operators.assignment() end

--[=[
	Always emits `+= 1` / `-= 1`. Prefix/postfix value is not preserved.

	**Syntax:** `i++;` · `i--;`

	@within Operators
	@tag operator
]=]
function Operators.increment() end

--[=[
	C++ designated initializer → Luau table field.

	**Syntax:** `Type { .Field = value }`

	**Emit:** `{ Field = value }`

	@within Operators
	@tag operator
]=]
function Operators.designated() end

return Operators
