--!strict
-- Compiled by CL++ — C++ × Luau

local unwrap: (number?) -> number
local init: () -> ()
-- option.server.clpp:1
function unwrap(value: number?): number
	do
		local __match1 = value
		if __match1 ~= nil then
			local n = __match1
			return n
		else
			return 0
		end
	end
end

-- option.server.clpp:2
function init()
	assert(unwrap(3) == 3)
	assert(unwrap(nil) == 0)
end

init()
