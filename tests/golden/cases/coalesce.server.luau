--!strict
-- Compiled by CL++ — C++ × Luau

local use: (number?) -> number
local guarded: (number?) -> ()
local init: () -> ()
-- coalesce.server.clpp:1
function use(value: number?): number
	return (function() local _t = value; return if _t ~= nil then _t else 5 end)()
end

-- coalesce.server.clpp:2
function guarded(value: number?)
	if not (value ~= nil) then
		return
	end
	assert(value ~= nil)
end

-- coalesce.server.clpp:6
function init()
	assert(use(nil) == 5)
	assert(use(2) == 2)
	guarded(nil)
	guarded(2)
end

init()
