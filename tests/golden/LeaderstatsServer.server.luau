-- Compiled by CL++ — C++ × Luau

const DataService = require(ClppLibs.DataService)
const Janitor = require(ClppLibs.Janitor)
const PlayerData = require(script.Parent.Parent.shared.PlayerData)

local LeaderstatsServer = {}

LeaderstatsServer.STARTING_COINS = 0

LeaderstatsServer.janitor = nil

-- LeaderstatsServer.server.clpp:9
function LeaderstatsServer:GetPlayerJanitorKey(player: Player): string
	return player.Name .. "_LeaderstatsJanitor"
end

-- LeaderstatsServer.server.clpp:13
function LeaderstatsServer:UpdateLeaderstatsWithValues(currentValue: IntValue, newValue: number)
	if not (currentValue ~= nil) then
		return
	end
	if currentValue.Value ~= newValue then
		currentValue.Value = newValue
	end
end

-- LeaderstatsServer.server.clpp:22
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
	coinsValue.Value = self.STARTING_COINS -- initial value; not a Changed event
	return newFolder
end

-- LeaderstatsServer.server.clpp:38
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
	self:UpdateLeaderstatsWithValues(coinsValue, newValue)
	print("Coins for " .. player.Name .. ": " .. newValue)
end), "Disconnect")
	end
	local key: string = self:GetPlayerJanitorKey(player)
	self.janitor:Add(leaderstatsFolder, "Destroy", key)
	self.janitor:Add(playerData, "Destroy", key)
end

-- LeaderstatsServer.server.clpp:61
const function init()
	local players: Players = game:GetService("Players")
	local leaderstatsServer: LeaderstatsServer = LeaderstatsServer
	local janitor: Janitor = Janitor.new()
	leaderstatsServer.janitor = janitor
	for _, player in players:GetPlayers() do
		leaderstatsServer:PlayerEntered(player)
	end
	janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	leaderstatsServer:PlayerEntered(playerEntered)
end), "Disconnect")
	game:BindToClose(function()
	janitor:Cleanup()
	janitor:Destroy()
end)
end

init()
