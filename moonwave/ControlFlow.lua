local ControlFlow = {}

--[=[
	@class ControlFlow
	Branching, loops, `guard`, `match`, and `switch`.

	There is no `continue`, ternary `? :`, or `do/while`. Full pages: [if](/CLPP/docs/reference/if), [guard](/CLPP/docs/reference/guard), [match](/CLPP/docs/reference/match).
]=]

--[=[
	C-style branch. `else if` emits Luau `elseif`.

	**Syntax:** `if (cond) { } else if (other) { } else { }`

	**Emit:** `if` / `elseif` / `else` / `end`

	Parentheses required. No ternary.

	@within ControlFlow
	@tag control
]=]
function ControlFlow.iff() end

--[=[
	Loop while the condition is true.

	**Syntax:** `while (cond) { }`

	**Emit:** `while … do … end`

	No `do/while`. No `continue`.

	@within ControlFlow
	@tag control
]=]
function ControlFlow.while_() end

--[=[
	C `for`. Emits `while` plus a trailing increment.

	**Syntax:** `for (int i = 0; i < n; i++) { }`

	**Emit:** `local i = 0` / `while i < n do` / `i += 1`

	Semicolons separate clauses. Range-for is a different form.

	@within ControlFlow
	@tag control
]=]
function ControlFlow.c_for() end

--[=[
	Range-for. The `:` here is not a table key.

	**Syntax:** `for (T x : list) { }`

	**Emit:** `for _, x in list do`

	```clpp
	for (Player* player : players::GetPlayers()) {
	    post(player.Name);
	}
	```

	@within ControlFlow
	@tag control
]=]
function ControlFlow.range_for() end

--[=[
	If the condition is false, the `else` block runs (usually `return`).

	**Syntax:** `guard (condition) else { return; }`

	**Emit:** `if not (condition) then … end`

	The `else` is required.

	```clpp
	guard (player != null) else {
	    warn("Invalid player");
	    return;
	}
	```

	@within ControlFlow
	@tag control
]=]
function ControlFlow.guard() end

--[=[
	Type switch. Instance types use `IsA`; primitives use `typeof`. `_` is the fallback.

	**Syntax:** `match (value) { Type name => stmt, _ => stmt };`

	**Emit:** `if x:IsA("Type") then` / `typeof`

	```clpp
	match (instance) {
	    Part* p => p.BrickColor = BrickColor::Red(),
	    _ => warn("unsupported")
	};
	```

	@within ControlFlow
	@tag control
]=]
function ControlFlow.match() end

--[=[
	Evaluates the discriminant once. `break` leaves the switch. No C fall-through; stacked `case`s share a body.

	**Syntax:** `switch (action) { case "buy": … break; default: … break; }`

	**Emit:** `if` / `elseif` inside `repeat … until true`

	@within ControlFlow
	@tag control
]=]
function ControlFlow.switch() end

--[=[
	Leaves the innermost loop or `switch`. There is no `continue`.

	**Syntax:** `break;`

	**Emit:** `break`

	@within ControlFlow
	@tag control
]=]
function ControlFlow.break_() end

--[=[
	Leave the current function.

	**Syntax:** `return;` · `return expr;`

	**Emit:** `return` / `return expr`

	@within ControlFlow
	@tag control
]=]
function ControlFlow.return_() end

return ControlFlow
