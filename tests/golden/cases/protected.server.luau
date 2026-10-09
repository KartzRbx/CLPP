--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
local Worker = {}
Worker.__index = Worker
export type Worker = typeof(setmetatable({} :: {  }, Worker))
function Worker.new(): Worker
	return setmetatable({
	}, Worker)
end

-- protected.server.clpp:2
function Worker:get(): number
	return 7
end

-- protected.server.clpp:3
function init()
	local w: Worker = Worker.new()
	local n = (function() local _ok, _r = pcall(function() return w:get() end); return if _ok then _r else nil end)()
	assert(n == 7)
end

init()
