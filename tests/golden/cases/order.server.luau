--!strict
-- Compiled by CL++ — C++ × Luau

local first: (number) -> number
local second: (number) -> number
local init: () -> ()
-- order.server.clpp:1
function first(x: number): number
	return second(x)
end

-- order.server.clpp:2
function second(x: number): number
	return x + 1
end

-- order.server.clpp:3
function init()
	assert(first(4) == 5)
end

init()
