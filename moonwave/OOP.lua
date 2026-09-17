local OOP = {}

--[=[
	@class OOP
	Declare the type in a `.clh`. Implement `Class::Method` in the sibling `.clp` / `.clpp`.

	`this` is `self`. Bare fields become `self.field`. `public:` / `private:` are ignored.
	There is no `int main()` — Scripts use `void init()`.
	Stems must match: `LeaderstatsServer.clh` beside `LeaderstatsServer.server.clpp`.
]=]

--[=[
	Type declaration. `class` is a synonym of `struct`.

	**Syntax:** `struct LeaderstatsServer { Janitor* janitor; };`

	**Emit:** `export type` plus methods; field-only headers emit `const function Name()` with defaults.

	Instances are not RAII. Leaving a block does not `Destroy` — use Janitor.

	@within OOP
]=]
function OOP.struct() end

--[=[
	Method implementation.

	**Syntax:** `void Class::Method(T arg) { this.field = arg; }`

	**Emit:** `function Class:Method(arg: T)`

	Untagged files with only `Class::` `return` the table (ModuleScript).

	@param arg any
	@within OOP
]=]
function OOP.method(arg) end

--[=[
	Construct **one** service in `init()` and use it from lambdas. That is the game singleton.

	**Syntax:** `void init() { LeaderstatsServer leaderstatsServer; }`

	@within OOP
]=]
function OOP.singleton() end

--[=[
	Parsed and ignored. Luau has no access specifiers.

	**Syntax:** `public:` · `private:` · `protected:`

	**Emit:** omitted

	@within OOP
]=]
function OOP.access() end

return OOP
