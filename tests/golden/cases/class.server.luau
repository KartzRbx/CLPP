--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
local HelloServer = {}
HelloServer.__index = HelloServer
export type HelloServer = typeof(setmetatable({} :: { value: number }, HelloServer))

HelloServer.value = 1

-- class.server.clpp:2
function HelloServer.new(): HelloServer
	local self: HelloServer = setmetatable({ value = 1 }, HelloServer)
	self.value = 7
	return self
end

-- class.server.clpp:3
function HelloServer:Greet(): number
	return self.value
end

-- class.server.clpp:4
function init()
	local hello: HelloServer = HelloServer.new()
	assert(hello:Greet() == 7)
end

init()
