--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
local Empty = {}
Empty.__index = Empty
export type Empty = typeof(setmetatable({} :: {  }, Empty))
function Empty.new(): Empty
	return setmetatable({
	}, Empty)
end

-- empty_class.server.clpp:2
function init()
	local a: Empty = Empty.new()
	local b: Empty = Empty.new()
	assert(a ~= b)
end

init()
