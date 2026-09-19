local OOP = {}

--[=[
	@class OOP
	Declare the type in a `.clh`. Implement `Class::Method` in the sibling `.clp` / `.clpp`.

	`@this` is `self`. `@field` and bare fields become `self.field`. `this` without `@` is the same alias. `public:` / `private:` are ignored.
	There is no `int main()` — Scripts use `void init()`.
	Stems must match: `LeaderstatsServer.clh` beside `LeaderstatsServer.server.clpp`.
]=]

--[=[
	Type declaration. `class` is a synonym of `struct`.

	**Syntax:** `struct LeaderstatsServer { Janitor* janitor; };`

	**Emit:** `export type` plus methods; field-only headers emit `const function Name()` with defaults.

	Instances are not RAII. Leaving a block does not `Destroy` — use Janitor.

	@function struct
	@within OOP
]=]

--[=[
	Method implementation.

	**Syntax:** `void Class::Method(T arg) { @field = arg; }`

	**Emit:** `function Class:Method(arg: T)`

	Untagged files with only `Class::` `return` the table (ModuleScript).

	@param arg any
	@function Class::Method
	@within OOP
]=]

--[=[
	Construct **one** service in `init()` and use it from lambdas. That is the game singleton.

	**Syntax:** `void init() { LeaderstatsServer leaderstatsServer; }`

	@function init
	@within OOP
]=]

--[=[
	Parsed and ignored. Luau has no access specifiers.

	**Syntax:** `public:` · `private:` · `protected:`

	**Emit:** omitted

	@function public
	@within OOP
]=]

return OOP
