--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
-- loops.server.clpp:1
function init()
	local n: number = 0
	for i = 0, (3) - 1, 1 do
		n += i
	end
	while n < 5 do
		n += 1
	end
	assert(n == 5)
end

init()
