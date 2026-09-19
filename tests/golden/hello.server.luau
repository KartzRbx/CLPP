-- Compiled by CL++ — C++ × Luau

const Janitor = require(ClppLibs.Janitor)

const function Greet(player: Player)
	print("Player name: " .. player.Name)
end

const function init()
	local __janitor = Janitor.new()
	local players: Players = game:GetService("Players")
	for _, player in players:GetPlayers() do
		Greet(player)
	end
	__janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	print("New player connected: " .. playerEntered.Name)
end), "Disconnect")
	game:BindToClose(function()
		__janitor:Cleanup()
		__janitor:Destroy()
	end)
end

init()
