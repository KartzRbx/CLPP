--!strict
-- Compiled by CL++ — C++ × Luau

local init: () -> ()
local Base = {}
Base.__index = Base
export type Base = typeof(setmetatable({} :: { value: number }, Base))
function Base.new(): Base
	return setmetatable({
		value = 3,
	}, Base)
end

local Derived = setmetatable({}, { __index = Base })
Derived.__index = Derived
export type Derived = typeof(setmetatable({} :: { value: number }, Derived))
function Derived.new(): Derived
	return setmetatable({
		value = 3,
	}, Derived)
end

Base.value = 3

-- inheritance.server.clpp:2
function Base:get(): number
	return self.value
end

-- inheritance.server.clpp:4
function init()
	local d: Derived = Derived.new()
	assert(d:get() == 3)
end

init()
