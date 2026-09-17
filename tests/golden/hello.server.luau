-- Compiled by CL++ — C++ × Luau

const function Greet(player: Player)
	print("Player name: " .. player.Name)
end

const function init()
	local players: Players = game:GetService("Players")
	for _, player in players:GetPlayers() do
		Greet(player)
	end
	players.PlayerAdded:Connect(function(playerEntered: Player)
	print("New player connected: " .. playerEntered.Name)
end)
end

init()
