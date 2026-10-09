--!strict
-- Compiled by CL++ — C++ × Luau

local value: (State) -> number
local init: () -> ()
local State = table.freeze({ Idle = "Idle" :: "Idle", Run = "Run" :: "Run" })
type State = "Idle" | "Run"

-- enum_match.server.clpp:2
function value(state: State): number
	do
		local __match1 = state
		if __match1 == State.Idle then
			return 1
		else
			return 2
		end
	end
end

-- enum_match.server.clpp:3
function init()
	assert(value(State.Idle) == 1)
	assert(value(State.Run) == 2)
end

init()
