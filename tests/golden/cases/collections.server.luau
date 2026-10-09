--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
-- collections.server.clpp:1
function init()
	local values: {number} = { 1, 2, 3 }
	local total: number = 0
	for _, value in values do
		total += value
	end
	assert(total == 6)
end

init()
