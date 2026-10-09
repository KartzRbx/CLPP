--!strict
-- Compiled by CL++ — C++ × Luau

local identity: <T>(T) -> T
local init: () -> ()
-- generics.server.clpp:1
function identity<T>(value: T): T
	return value
end

-- generics.server.clpp:2
function init()
	assert(identity(3) == 3)
end

init()
