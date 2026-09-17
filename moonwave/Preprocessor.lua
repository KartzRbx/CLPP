local Preprocessor = {}

--[=[
	@class Preprocessor
	`#include` and `#pragma`. There are no general macros (`#define` is not a feature).

	`#pragma` lines are stripped before parse, so they are valid in `.clh`, `.clp`, and `.clpp`.
]=]

--[=[
	Angle-bracket includes are IntelliSense (and `require` for libs). Quoted sibling stem is inlined.

	**Syntax:** `#include <clpp/roblox.clh>` · `#include "LeaderstatsServer.clh"`

	@within Preprocessor
]=]
function Preprocessor.include() end

--[=[
	Include guard. Headers listed more than once are skipped. Valid at the top of every `.clh`.

	**Syntax:** `#pragma once`

	**Emit:** (none)

	@within Preprocessor
]=]
function Preprocessor.once() end

--[=[
	Emit `--!strict` at the top of the Luau file.

	**Syntax:** `#pragma strict`

	**Emit:** `--!strict`

	@within Preprocessor
]=]
function Preprocessor.strict() end

--[=[
	Emit `--!nonstrict`. Aliases: `nstrict`, `nonstrict`.

	**Syntax:** `#pragma nostrict`

	**Emit:** `--!nonstrict`

	@within Preprocessor
]=]
function Preprocessor.nostrict() end

--[=[
	Emit `--!native` (Luau native codegen).

	**Syntax:** `#pragma native`

	**Emit:** `--!native`

	@within Preprocessor
]=]
function Preprocessor.native() end

--[=[
	Emit `--!optimize N`. Bare `optimize` means `2`.

	**Syntax:** `#pragma optimize` · `#pragma optimize 2` · `#pragma optimize 1` · `#pragma optimize 0`

	**Emit:** `--!optimize 2`

	@within Preprocessor
]=]
function Preprocessor.optimize() end

return Preprocessor
