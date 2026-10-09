--!strict
-- Compiled by CL++ — C++ × Luau

local fetch: (boolean) -> { tag: "Ok", ok: boolean } | { tag: "Err", err: string? }
local pass: (boolean) -> { tag: "Ok", ok: boolean } | { tag: "Err", err: string? }
local inspect: ({ tag: "Ok", ok: boolean } | { tag: "Err", err: string? }) -> number
local init: () -> ()
-- result_false.server.clpp:1
function fetch(good: boolean): { tag: "Ok", ok: boolean } | { tag: "Err", err: string? }
	if good then
		return { tag = "Ok", ok = false }
	end
	return { tag = "Err", err = nil }
end

-- result_false.server.clpp:2
function pass(good: boolean): { tag: "Ok", ok: boolean } | { tag: "Err", err: string? }
	local __result1 = fetch(good)
	if __result1.tag == "Err" then
		return __result1
	end
	local value: boolean = __result1.ok
	return { tag = "Ok", ok = value }
end

-- result_false.server.clpp:3
function inspect(value: { tag: "Ok", ok: boolean } | { tag: "Err", err: string? }): number
	do
		local __match1 = value
		if __match1.tag == "Ok" then
			local n = __match1.ok
			return (if n then 1 else 2)
		else
			local e = __match1.err
			return 3
		end
	end
end

-- result_false.server.clpp:4
function init()
	assert(inspect(pass(true)) == 2)
	assert(inspect(pass(false)) == 3)
end

init()
