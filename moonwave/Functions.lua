local Functions = {}

--[=[
	@class Functions
	Named functions, `func (…)` callbacks, and `@this`. No overloading. No default arguments. No C++ captures.
]=]

--[=[
	Named function. Only bodies in `.clp` / `.clpp` emit. Prototypes in `.clh` become `export type` fields.

	**Syntax:** `ReturnType Name(T arg) { return arg; }`

	**Emit:** `const function Name(arg: T): ReturnType`

	```clpp
	void CreateLeaderstats(Player* player) {
	    return;
	}
	```

	@function function
	@within Functions
]=]

--[=[
	Closure. `func (params) { }` — no C++ captures. Luau still closes over outer locals.

	**Syntax:** `func (T arg) { }` · `func cb = func () {};`

	**Emit:** `function(arg: T) … end`

	```clpp
	players.PlayerAdded~>Connect(func (Player* playerEntered) {
	    post("New player: " .: playerEntered.Name);
	});
	```

	@param arg any -- Lambda parameters
	@function lambda
	@within Functions
]=]

--[=[
	The current object inside `Class::Method`. Emits `self`.

	**Syntax:** `@this` · `@field` · `this` (alias) · bare `field` → `self.field`

	There is no `this->`. Only valid inside `Class::Method`.

	@function this
	@within Functions
]=]

return Functions
