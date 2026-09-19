local Observables = {}

--[=[
	@class Observables
	`observable T name = value` becomes a ValueBase (`IntValue`, `NumberValue`, `StringValue`, `BoolValue`, or `ObjectValue`).

	Reading or writing the identifier uses `.Value`. Assigning **fires** `Changed`.
	`.OnChange(fn)` is `Changed:Connect(fn)`.

	Full page: [observable](/CLPP/docs/reference/observable).
]=]

--[=[
	Declare a ValueBase-backed variable.

	**Syntax:** `observable T name = value;`

	**Emit:** `Instance.new("IntValue"); name.Value = value`

	`int` → IntValue, `float`/`double` → NumberValue, `string` → StringValue, `bool` → BoolValue, else ObjectValue.

	@function observable
	@within Observables
	@tag signal
]=]

--[=[
	Listen to value changes. Argument is the new `.Value`.

	**Syntax:** `name.OnChange(fn);`

	**Emit:** `name.Changed:Connect(fn)`

	```clpp
	observable int coins = 100;
	coins.OnChange(func (int newValue) {
	    post("now " .: newValue);
	});
	coins = 50;
	```

	@param handler func -- Listener
	@within Observables
	@tag signal
]=]
function Observables.OnChange(handler) end

return Observables
