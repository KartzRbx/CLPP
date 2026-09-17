local Functions = {}

--[=[
	@class Functions
	Named functions, lambdas, and `this`. No overloading. No default arguments. No C++ captures.
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

	@within Functions
]=]
function Functions.declare() end

--[=[
	Closure. Empty `[]` only — no `[x]` / `[&]`. Luau still closes over outer locals.

	**Syntax:** `func [](T arg) { }` · `func cb = []() {};`

	**Emit:** `function(arg: T) … end`

	```clpp
	players::PlayerAdded::Connect(func [](Player* playerEntered) {
	    post("New player: " .: playerEntered.Name);
	});
	```

	@param arg any -- Lambda parameters
	@within Functions
]=]
function Functions.lambda(arg) end

--[=[
	The current object inside `Class::Method`. Emits `self`.

	**Syntax:** `this.field` · bare `field` → `self.field`

	There is no `this->`.

	@within Functions
]=]
function Functions.this() end

return Functions
