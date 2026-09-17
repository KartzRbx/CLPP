local Preprocessor = {}

--[=[
	@class Preprocessor
	`#include` and `#pragma`. There are no general macros (`#define` is not a feature).

	`#pragma` lines are stripped before parse, so they are valid in `.clh`, `.clp`, and `.clpp`.
]=]

--[=[
	Angle-bracket includes are IntelliSense (and `require` for libs). Quoted sibling stem is inlined.

	**Syntax:** `#include <clpp/roblox.clh>` · `#include "LeaderstatsServer.clh"`

	@function include
	@within Preprocessor
]=]

--[=[
	Include guard. Headers listed more than once are skipped. Valid at the top of every `.clh`.

	**Syntax:** `#pragma once`

	**Emit:** (none)

	@function pragma-once
	@within Preprocessor
]=]

--[=[
	Emit `--!strict` at the top of the Luau file.

	**Syntax:** `#pragma strict`

	**Emit:** `--!strict`

	@function pragma-strict
	@within Preprocessor
]=]

--[=[
	Emit `--!nonstrict`. Aliases: `nstrict`, `nonstrict`.

	**Syntax:** `#pragma nostrict`

	**Emit:** `--!nonstrict`

	@function pragma-nostrict
	@within Preprocessor
]=]

--[=[
	Emit `--!native` (Luau native codegen).

	**Syntax:** `#pragma native`

	**Emit:** `--!native`

	@function pragma-native
	@within Preprocessor
]=]

--[=[
	Emit `--!optimize N`. Bare `optimize` means `2`.

	**Syntax:** `#pragma optimize` · `#pragma optimize 2` · `#pragma optimize 1` · `#pragma optimize 0`

	**Emit:** `--!optimize 2`

	@function pragma-optimize
	@within Preprocessor
]=]

return Preprocessor
