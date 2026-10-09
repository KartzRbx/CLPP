--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
-- lambda.server.clpp:1
function init()
	if true then
		local callback = function(n: number)
			if n > 0 then
				print(n)
			end
		end
		callback(2)
	end
end

init()
