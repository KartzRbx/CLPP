local Types = {}

--[=[
	@class Types
	Every value you write in CL++ has a type, and the compiler emits a Luau annotation.

	Full pages: [int](/CLPP/docs/reference/int), [array](/CLPP/docs/reference/array), [signal](/CLPP/docs/reference/signal-type), [observable](/CLPP/docs/reference/observable).
]=]

--[=[
	IEEE-looking integer in source. Luau numbers are doubles.

	**Syntax:** `int name = 0;`

	**Emit:** `number`

	`observable int` → `IntValue`. Always initialize: `int coins;` emits `nil`.

	@within Types
	@tag type
]=]
function Types.int() end

--[=[
	Floating-point number. Same emit as `int` and `double`.

	**Syntax:** `float speed = 16.5;`

	**Emit:** `number`

	`observable float` → `NumberValue`.

	@within Types
	@tag type
]=]
function Types.float() end

--[=[
	Same emit as `float`.

	**Syntax:** `double alpha = 0.25;`

	**Emit:** `number`

	@within Types
	@tag type
]=]
function Types.double() end

--[=[
	Boolean. Literals `true` / `false`. `!` emits `not`.

	**Syntax:** `bool isActive = true;`

	**Emit:** `boolean`

	@within Types
	@tag type
]=]
function Types.bool() end

--[=[
	UTF-8 text. Not `std::string`. Concatenate with `.:`.

	**Syntax:** `string name = "Kartz";`

	**Emit:** `string`

	@within Types
	@tag type
]=]
function Types.string() end

--[=[
	No value. Return type of procedures. `void init()` is the script entry.

	**Syntax:** `void Name(T arg) { return; }`

	**Emit:** no return annotation

	@within Types
	@tag type
]=]
function Types.void() end

--[=[
	Function type and optional lambda prefix.

	**Syntax:** `func cb = [](int n) { };`

	**Emit:** `(...any) -> any`

	@within Types
	@tag type
]=]
function Types.func() end

--[=[
	Infer the type from the initializer.

	**Syntax:** `auto* players = GetService<Players>();`

	Inferred for `new`, `GetService<T>()`, datatype constructors, and destructuring.

	@within Types
	@tag type
]=]
function Types.auto() end

--[=[
	A value that may be missing.

	**Syntax:** `optional<T> name = null;`

	**Emit:** `T?`

	@within Types
	@tag type
]=]
function Types.optional() end

--[=[
	`Player*` means an Instance of class Player — not a heap pointer.

	**Syntax:** `Player* player = null;`

	**Emit:** star stripped (`Player`)

	Properties use `.`. Methods use `::`. No `delete`, `*p`, `&p`, `int&`, or `->`.

	@within Types
	@tag type
]=]
function Types.instance_pointer() end

--[=[
	`const` / `static constexpr` become Luau `const`.

	**Syntax:** `const int DoubleCoins(int coins) { return coins; }`

	**Emit:** `const function`

	@within Types
	@tag type
]=]
function Types.const() end

--[=[
	Source-level cast. Does not check `ClassName`.

	**Syntax:** `T static_cast<T>(x);`

	**Emit:** `x`

	@param x any
	@within Types
	@tag type
]=]
function Types.static_cast(x) end

return Types
