local Attributes = {}

--[=[
	@class Attributes
	`[[server]]` and `[[client]]` mark functions that only exist on one peer.

	On a `.server.clpp` file, `[[client]]` functions are omitted.
	On a `.client.clpp` file, `[[server]]` functions are omitted.
	In a module, emit wraps them in `RunService:IsServer()` / `IsClient()`.
]=]

--[=[
	Runs on the server only.

	**Syntax:** `[[server]] void SaveData(Player* player) { }`

	**Emit:** omitted on client files; `IsServer()` wrapper in modules

	@within Attributes
]=]
function Attributes.server() end

--[=[
	Runs on the client only.

	**Syntax:** `[[client]] void UpdateUI() { }`

	**Emit:** omitted on server files; `IsClient()` wrapper in modules

	@within Attributes
]=]
function Attributes.client() end

return Attributes
