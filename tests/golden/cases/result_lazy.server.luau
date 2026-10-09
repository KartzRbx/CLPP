--!strict
-- Compiled by CL++ — C++ × Luau

local fetch: (boolean) -> { tag: "Ok", ok: number } | { tag: "Err", err: string }
local select: (boolean) -> { tag: "Ok", ok: number } | { tag: "Err", err: string }
local inspect: ({ tag: "Ok", ok: number } | { tag: "Err", err: string }) -> number
local init: () -> ()
-- result_lazy.server.clpp:1
function fetch(good: boolean): { tag: "Ok", ok: number } | { tag: "Err", err: string }
	if good then
		return { tag = "Ok", ok = 8 }
	end
	return { tag = "Err", err = "error" }
end

-- result_lazy.server.clpp:2
function select(pick: boolean): { tag: "Ok", ok: number } | { tag: "Err", err: string }
	local __value3
	if pick then
		local __result1 = fetch(true)
		if __result1.tag == "Err" then
			return __result1
		end
		__value3 = __result1.ok
	else
		local __result2 = fetch(false)
		if __result2.tag == "Err" then
			return __result2
		end
		__value3 = __result2.ok
	end
	local value: number = __value3
	return { tag = "Ok", ok = value }
end

-- result_lazy.server.clpp:3
function inspect(value: { tag: "Ok", ok: number } | { tag: "Err", err: string }): number
	do
		local __match1 = value
		if __match1.tag == "Ok" then
			local n = __match1.ok
			return n
		else
			local e = __match1.err
			return -1
		end
	end
end

-- result_lazy.server.clpp:4
function init()
	assert(inspect(select(true)) == 8)
	assert(inspect(select(false)) == (-1))
end

init()
