local Signals = {}

--[=[
	@class Signals
	`signal<T...>` is a typed BindableEvent (`__signal()` in emitted Luau).

	| CL++ | Meaning | Page |
	| --- | --- | --- |
	| `name::Fire(...)` | send | [Fire](/CLPP/docs/reference/Fire) |
	| `name~>Connect(fn)` | listen (Janitor) | [Connect](/CLPP/docs/reference/Connect) |
	| `name~>Once(fn)` | listen once (Janitor) | [Once](/CLPP/docs/reference/Once) |
	| `name::Wait()` | yield until the next fire | [Wait](/CLPP/docs/reference/Wait) |
	| `obj::GetPropertyChangedSignal("X")` | Instance property changes | [GetPropertyChangedSignal](/CLPP/docs/reference/GetPropertyChangedSignal) |
]=]

--[=[
	Declare a typed signal.

	**Syntax:** `signal<Player*, int> OnCoinsUpdated;`

	**Emit:** `local OnCoinsUpdated = __signal()`

	@function signal<T>
	@within Signals
	@tag signal
]=]

--[=[
	Emits to every current listener.

	**Syntax:** `name::Fire(...);`

	**Emit:** `name:Fire(...)`

	```clpp
	signal<Player*, int> OnCoinsUpdated;
	OnCoinsUpdated::Fire(player, 500);
	```

	@param ... any -- Payload matching `signal<T...>`
	@within Signals
	@tag signal
]=]
function Signals.Fire(...) end

--[=[
	Subscribe until Disconnect. Prefer `~>Connect` so Janitor owns the connection.

	**Syntax:** `name::Connect(fn);` · `name~>Connect(fn);`

	**Emit:** `name:Connect(fn)` or `janitor:Add(..., "Disconnect")`

	@param handler func -- Listener
	@within Signals
	@tag signal
]=]
function Signals.Connect(handler) end

--[=[
	Subscribe for a **single** emission, then disconnect. `~>Once` is janitor-managed.

	**Syntax:** `name::Once(fn);` · `name~>Once(fn);`

	**Emit:** `name:Once(fn)`

	@param handler func -- Listener
	@within Signals
	@tag signal
]=]
function Signals.Once(handler) end

--[=[
	Yields until the next `Fire`.

	**Syntax:** `name::Wait();`

	**Emit:** `name:Wait()`

	@within Signals
	@tag signal
]=]
function Signals.Wait() end

--[=[
	Engine signal for one Instance property.

	**Syntax:** `obj::GetPropertyChangedSignal("Name")~>Connect(fn);`

	**Emit:** `obj:GetPropertyChangedSignal("Name")`

	@param name string -- Property name
	@within Signals
	@tag signal
]=]
function Signals.GetPropertyChangedSignal(name) end

return Signals
