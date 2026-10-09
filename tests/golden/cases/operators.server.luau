--!strict
-- Compiled by CL++ — C++ × Luau

local calc: (number) -> number
local init: () -> ()
-- operators.server.clpp:1
function calc(x: number): number
	return (x * 2) + 1
end

-- operators.server.clpp:2
function init()
	assert(calc(4) == 9)
	local s: string = "coins=" .. tostring(3)
	assert(s == "coins=3")
end

init()
