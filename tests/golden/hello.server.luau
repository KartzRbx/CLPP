-- Compiled by CL++ — C++ × Luau

const Janitor = require(ClppLibs.Janitor)

local HelloServer = {}

-- hello.server.clpp:7
function HelloServer:Greet(player: Player)
	print("Player name: " .. player.Name)
end

-- hello.server.clpp:11
const function init()
	local __janitor = Janitor.new()
	local hello: HelloServer = HelloServer
	local players: Players = game:GetService("Players")
	for _, player in players:GetPlayers() do
		hello:Greet(player)
	end
	__janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	hello:Greet(playerEntered)
end), "Disconnect")
	game:BindToClose(function()
		__janitor:Cleanup()
		__janitor:Destroy()
	end)
end

init()
