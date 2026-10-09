--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
-- precedence.server.clpp:1
function init()
	local n: number = (1 + 2) * 3
	assert(n == 9)
	local value: boolean = not (true or false)
	assert(not value)
	local difference: number = 9 - (3 - 1)
	assert(difference == 7)
end

init()
