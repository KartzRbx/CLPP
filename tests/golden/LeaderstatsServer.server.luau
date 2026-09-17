-- Compiled by CL++ — C++ × Luau

const DataService = require(ClppLibs.DataService)
const Janitor = require(ClppLibs.Janitor)
const PlayerData = require(script.Parent.Parent.shared.PlayerData)

local LeaderstatsServer = {}

LeaderstatsServer.STARTING_COINS = 0

LeaderstatsServer.dataService = nil

LeaderstatsServer.players = nil

LeaderstatsServer.janitor = nil

function LeaderstatsServer:GetPlayerJanitorKey(player: Player): string
	return player.Name .. "_LeaderstatsJanitor"
end

function LeaderstatsServer:UpdateLeaderstatsWithValues(currentValue: IntValue, newValue: number)
	if currentValue.Value ~= newValue then
		currentValue.Value = newValue
	end
end

function LeaderstatsServer:EnsurePlayerLeaderstatsFolder(player: Player): Folder
	local existingFolder: Instance = player:FindFirstChild("leaderstats")
	if existingFolder then
		return existingFolder
	end
	local newFolder: Folder = Instance.new("Folder")
	newFolder.Parent = player
	newFolder.Name = "leaderstats"
	local coinsValue: IntValue = Instance.new("IntValue")
	coinsValue.Parent = newFolder
	coinsValue.Name = "Coins"
	coinsValue.Value = self.STARTING_COINS
	return newFolder
end

function LeaderstatsServer:PlayerEntered(player: Player)
	if not (player ~= nil) then
		warn("Invalid player")
		return
	end
	local leaderstatsFolder: Folder = self:EnsurePlayerLeaderstatsFolder(player)
	local playerData: Data = DataService.Server:WaitFor(player)
	local Paths: PlayerData = DataService.Paths
	local coinsValue: IntValue = leaderstatsFolder:FindFirstChild("Coins")
	if coinsValue then
		self.janitor:Add(playerData:GetChangedSignal(Paths.Currencies.Coins):Connect(function(newValue: number)
	if coinsValue.Value ~= newValue then
		coinsValue.Value = newValue
		print("Coins for " .. player.Name .. ": " .. newValue)
	end
end), "Disconnect")
	end
	self.janitor:Add(leaderstatsFolder, "Destroy", self:GetPlayerJanitorKey(player))
	self.janitor:Add(playerData, "Destroy", self:GetPlayerJanitorKey(player))
end

const function init()
	local players: Players = game:GetService("Players")
	local leaderstatsServer: LeaderstatsServer = LeaderstatsServer
	local janitor: Janitor = leaderstatsServer.janitor
	for _, player in players:GetPlayers() do
		leaderstatsServer:PlayerEntered(player)
	end
	janitor:Add(players.PlayerAdded:Once(function(playerEntered: Player)
	leaderstatsServer:PlayerEntered(playerEntered)
end), "Disconnect")
	game:BindToClose(function()
	janitor:Cleanup()
	janitor:Destroy()
end)
end

init()
