-- Compiled by CL++ — C++ × Luau

const DataService = require(ClppLibs.DataService)
const Janitor = require(ClppLibs.Janitor)

export type LeaderstatsServer = {
	STARTING_COINS: number,
	dataService: DataServiceServer,
	players: Players,
	janitor: Janitor,
	GetPlayerJanitorKey: (self: LeaderstatsServer, player: Player) -> string,
	EnsurePlayerLeaderstatsFolder: (self: LeaderstatsServer, player: Player) -> Folder,
	PlayerEntered: (self: LeaderstatsServer, player: Player),
	UpdateLeaderstatsWithValues: (self: LeaderstatsServer, currentValue: IntValue, newValue: number),
}

return {}
