local Concurrency = {}

--[=[
	@class Concurrency
	`async` / `await`, `spawn`, `parallel`, and multiple-return destructuring.
]=]

--[=[
	Marks a function that may `await`.

	**Syntax:** `async T Name(Args args) { T x = await expr; return x; }`

	**Emit:** `const function` whose body uses `__await`

	@within Concurrency
]=]
function Concurrency.async() end

--[=[
	Wait for a Promise-like value inside an `async` function.

	**Syntax:** `T x = await expr;`

	**Emit:** `__await(expr)`

	If the value has `:expect()` (Promise), wait; otherwise return it.

	```clpp
	async Data* FetchData(Player* player) {
	    Data* data = await DataService:Server::WaitFor(player);
	    return data;
	}
	```

	@within Concurrency
]=]
function Concurrency.await() end

--[=[
	Run a block on a new thread.

	**Syntax:** `spawn { task::wait(2); post("done"); };`

	**Emit:** `task.spawn(function() … end)`

	@within Concurrency
]=]
function Concurrency.spawn() end

--[=[
	Parallel Luau block.

	**Syntax:** `parallel { ComputeComplexPhysics(); };`

	**Emit:** `task.desynchronize()` / `task.synchronize()`

	@within Concurrency
]=]
function Concurrency.parallel() end

--[=[
	Unpack multiple return values. Usual pairing with `pcall`.

	**Syntax:** `auto [a, b] = expr;`

	**Emit:** `local a, b = expr`

	@within Concurrency
]=]
function Concurrency.destructure() end

return Concurrency
