--!strict
-- Compiled by CL++ — C++ × Luau

local convert: (number) -> number
local init: () -> ()
-- switch.server.clpp:1
function convert(value: number): number
	repeat
		if value == 1 then
			return 3
		else
			return 4
		end
	until true
end

-- switch.server.clpp:2
function init()
	assert(convert(1) == 3)
	assert(convert(2) == 4)
end

init()
