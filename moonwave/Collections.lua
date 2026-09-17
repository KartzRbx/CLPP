local Collections = {}

--[=[
	@class Collections
	Arrays are ordered lists. Dictionaries are maps. Both become Luau tables.

	Aliases: `vector<T>`, `LuaArray<T>`, `span<T>` → `{T}`. `map<K,V>` → `{ [K]: V }`.
]=]

--[=[
	Ordered list.

	**Syntax:** `array<T> name = { a, b, c };`

	**Emit:** `{T}`

	```clpp
	array<string> names = {"Kartz", "Player1"};
	for (string n in names) {
	    post(n);
	}
	```

	Iterate with range-for. `Vector3` is a datatype, not `vector<T>`.

	@within Collections
]=]
function Collections.array() end

--[=[
	Alias of `array<T>`. Not `Vector3`.

	**Syntax:** `vector<T> name = { a, b };`

	**Emit:** `{T}`

	@within Collections
]=]
function Collections.vector() end

--[=[
	String-keyed (or typed-key) map. Access keys with `:`, not `.`.

	**Syntax:**
	```clpp
	dictionary<string, int> stats = {
	    {"Coins", 100},
	    {"Gems", 50}
	};
	post(stats:Coins);
	```

	**Emit:** `{ [K]: V }` with `Key = value` fields

	@within Collections
]=]
function Collections.dictionary() end

return Collections
