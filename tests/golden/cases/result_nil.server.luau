--!strict
-- Compiled by CL++ — C++ × Luau

local fetch: () -> { tag: "Ok", ok: number? } | { tag: "Err", err: string }
local unwrap: ({ tag: "Ok", ok: number? } | { tag: "Err", err: string }) -> number
local init: () -> ()
-- result_nil.server.clpp:1
function fetch(): { tag: "Ok", ok: number? } | { tag: "Err", err: string }
	return { tag = "Ok", ok = nil }
end

-- result_nil.server.clpp:2
function unwrap(value: { tag: "Ok", ok: number? } | { tag: "Err", err: string }): number
	do
		local __match1 = value
		if __match1.tag == "Ok" then
			local n = __match1.ok
			return (function() local _t = n; return if _t ~= nil then _t else 10 end)()
		else
			local e = __match1.err
			return -1
		end
	end
end

-- result_nil.server.clpp:3
function init()
	assert(unwrap(fetch()) == 10)
end

init()
