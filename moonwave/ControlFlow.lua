local ControlFlow = {}

--[=[
	@class ControlFlow
	Branching, loops, `guard`, `match`, and `switch`.

	There is no `continue`, ternary `? :`, or `do/while`. Full pages: [if](/CLPP/docs/reference/if), [guard](/CLPP/docs/reference/guard), [match](/CLPP/docs/reference/match).
]=]

--[=[
	C-style branch. `else if` emits Luau `elseif`. Parentheses required. No ternary.

	**Syntax:** `if (cond) { } else if (other) { } else { }`

	**Emit:** `if` / `elseif` / `else` / `end`

	@function if
	@within ControlFlow
	@tag control
]=]

--[=[
	Loop while the condition is true. No `do/while`. No `continue`.

	**Syntax:** `while (cond) { }`

	**Emit:** `while … do … end`

	@function while
	@within ControlFlow
	@tag control
]=]

--[=[
	C `for`. Emits `while` plus a trailing increment. Semicolons separate clauses.

	**Syntax:** `for (int i = 0; i < n; i++) { }`

	**Emit:** `local i = 0` / `while i < n do` / `i += 1`

	@function for
	@within ControlFlow
	@tag control
]=]

--[=[
	Range-for. Prefer `in`. The C++-style `:` is the same loop, not a table key.

	**Syntax:** `for (T x in list) { }` · `for (T x in list) { }`

	**Emit:** `for _, x in list do`

	```clpp
	for (Player player in players.GetPlayers()) {
	    post(player.Name);
	}
	```

	@function for-in
	@within ControlFlow
	@tag control
]=]

--[=[
	If the condition is false, the `else` block runs (usually `return`). The `else` is required.

	**Syntax:** `guard (condition) else { return; }`

	**Emit:** `if not (condition) then … end`

	```clpp
	guard (player != null) else {
	    warn("Invalid player");
	    return;
	}
	```

	@function guard
	@within ControlFlow
	@tag control
]=]

--[=[
	Type switch. Instance types use `IsA`; primitives use `typeof`. `_` is the fallback.

	**Syntax:** `match (value) { Type name => stmt, _ => stmt };`

	**Emit:** `if x:IsA("Type") then` / `typeof`

	Arms are a class or primitive name (`Part p`, `string s`) — not a C++ pointer.

	```clpp
	match (instance) {
	    Part p => p.BrickColor = BrickColor::Red(),
	    _ => warn("unsupported")
	};
	```

	@function match
	@within ControlFlow
	@tag control
]=]

--[=[
	Evaluates the discriminant once. `break` leaves the switch. No C fall-through; stacked `case`s share a body.

	**Syntax:** `switch (action) { case "buy": … break; default: … break; }`

	**Emit:** `if` / `elseif` inside `repeat … until true`

	@function switch
	@within ControlFlow
	@tag control
]=]

--[=[
	Leaves the innermost loop or `switch`. There is no `continue`.

	**Syntax:** `break;`

	**Emit:** `break`

	@function break
	@within ControlFlow
	@tag control
]=]

--[=[
	Leave the current function.

	**Syntax:** `return;` · `return expr;`

	**Emit:** `return` / `return expr`

	@function return
	@within ControlFlow
	@tag control
]=]

return ControlFlow
