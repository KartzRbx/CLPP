local Builtins = {}

--[=[
	@class Builtins
	Global functions that exist in every CL++ program. You do not import these.

	Each entry below is a full reference: syntax, parameters, return value, Luau emit, and example.
	The header-style index is [Language reference](/CLPP/docs/reference).
]=]

--[=[
	Writes values to the output log. Analog of C `printf` / Luau `print`.

	**Syntax:** `void post(...);`

	**Return:** `void`

	**Emit:** `print(...)`

	```clpp
	post("hello");
	post("coins:", 100);
	post("online: " .: players::GetPlayers());
	```

	@param ... any -- Values to print
	@within Builtins
	@tag io
]=]
function Builtins.post(...) end

--[=[
	Yellow warning. Does not stop the thread.

	**Syntax:** `void warn(...);`

	**Emit:** `warn(...)`

	Pair with `guard` when the failure is expected.

	@param ... any -- Warning payload
	@within Builtins
	@tag io
]=]
function Builtins.warn(...) end

--[=[
	Throws. Analog of C++ `throw` / Luau `error`. The current thread stops.

	**Syntax:** `void report(...);`

	**Return:** does not return

	**Emit:** `error(...)`

	There is no `try/catch`. Catch with `pcall`.

	@param ... any -- Error payload
	@within Builtins
	@tag io
]=]
function Builtins.report(...) end

--[=[
	Looks up a Roblox service by class name. The only generic besides collections and `static_cast`.

	**Syntax:** `T* GetService<T>();`

	**Return:** Instance of class `T`

	**Emit:** `game:GetService("T")`

	```clpp
	Players* players = GetService<Players>();
	```

	There is no `game:GetService` in CL++ source.

	@function GetService<T>
	@within Builtins
]=]

--[=[
	Protected call. CL++ has no `try/catch`.

	**Syntax:** `auto [ok, result] = pcall(fn);`

	**Return:** success flag, then result or error string

	**Emit:** `pcall(fn)`

	@param fn func -- Callback to run
	@within Builtins
]=]
function Builtins.pcall(fn) end

--[=[
	Constructs a Roblox Instance or a library object. Not C++ heap allocation — there is no `delete`.

	**Syntax:** `auto* child = new Class(parent);`

	**Emit:** `Instance.new("Class")` for Instances (first arg → `.Parent`); `Janitor.new()` for libs.

	Datatypes (`Vector3`, `CFrame`) are values: write `Vector3(8, 1, 8)`, not `new Vector3`.

	@within Builtins
	@tag constructor
]=]
function Builtins.new() end

--[=[
	Source-level type annotation. Luau has no runtime casts — this does not check `ClassName`.

	**Syntax:** `T static_cast<T>(value);`

	**Emit:** `value` with a Luau annotation

	Use `match` to branch on Instance class at runtime.

	@param value any -- Expression to re-annotate
	@within Builtins
]=]
function Builtins.static_cast(value) end

--[=[
	Variadic string join. Prefer `.:` for two operands.

	**Syntax:** `string string_concat(...);`

	**Return:** `string`. Zero args → `""`. One arg → that value.

	**Emit:** `(a .. b .. c)`

	Do not write Lua `..` and do not use `+` on strings.

	@param ... any -- Values to join
	@within Builtins
]=]
function Builtins.string_concat(...) end

--[=[
	Convert any value to text. Analog of C++ `std::to_string` / Luau `tostring`.

	**Syntax:** `string to_string(value);`

	**Emit:** `tostring(value)`

	Do not write Luau `tostring` in CL++ source.

	@param value any -- Value to convert
	@within Builtins
]=]
function Builtins.to_string(value) end

--[=[
	Parse a number from text (or pass a number through). Analog of `std::stod` / Luau `tonumber`.

	**Syntax:** `float to_number(value);` · `float to_number(value, base);`

	**Emit:** `tonumber(value)`

	Fails → `null`. Do not write Luau `tonumber` in CL++ source.

	@param value any -- Text or number
	@within Builtins
]=]
function Builtins.to_number(value) end

--[=[
	Coerce to boolean. Anything truthy becomes `true`.

	**Syntax:** `bool to_bool(value);`

	**Emit:** `not not (value)`

	@param value any -- Value to coerce
	@within Builtins
]=]
function Builtins.to_bool(value) end

--[=[
	Absence of a value. `nullptr` is a synonym.

	**Syntax:** `T* ref = null;`

	**Emit:** `nil`

	Uninitialized locals (`int coins;`) also emit `nil` — always initialize.

	@within Builtins
]=]
function Builtins.null() end

--[=[
	Script / LocalScript entry. There is no `int main()`.

	**Syntax:** `void init() { }`

	**Emit:** `init()` called at the end of the file

	Construct one service object in `init()` and close over it from lambdas.

	@within Builtins
]=]
function Builtins.init() end

return Builtins
