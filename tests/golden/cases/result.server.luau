--!strict
-- Compiled by CL++ — C++ × Luau

local fetch: (boolean) -> { tag: "Ok", ok: number } | { tag: "Err", err: string }
local doubled: (boolean) -> { tag: "Ok", ok: number } | { tag: "Err", err: string }
local unwrap: ({ tag: "Ok", ok: number } | { tag: "Err", err: string }) -> number
local init: () -> ()
-- result.server.clpp:1
function fetch(good: boolean): { tag: "Ok", ok: number } | { tag: "Err", err: string }
	if good then
		return { tag = "Ok", ok = 4 }
	end
	return { tag = "Err", err = "failed" }
end

-- result.server.clpp:2
function doubled(good: boolean): { tag: "Ok", ok: number } | { tag: "Err", err: string }
	local __result1 = fetch(good)
	if __result1.tag == "Err" then
		return __result1
	end
	local n: number = __result1.ok
	return { tag = "Ok", ok = n * 2 }
end

-- result.server.clpp:3
function unwrap(value: { tag: "Ok", ok: number } | { tag: "Err", err: string }): number
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

-- result.server.clpp:4
function init()
	assert(unwrap(doubled(true)) == 8)
	assert(unwrap(doubled(false)) == (-1))
end

init()
