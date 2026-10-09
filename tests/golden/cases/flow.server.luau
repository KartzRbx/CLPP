--!strict
-- Compiled by CL++ — C++ × Luau

local absolute: (number) -> number
local init: () -> ()
-- flow.server.clpp:1
function absolute(x: number): number
	if x < 0 then
		return -x
	else
		return x
	end
end

-- flow.server.clpp:2
function init()
	assert(absolute(-4) == 4)
end

init()
