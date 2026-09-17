local Preprocessor = {}

--[=[
	@class Preprocessor
	`#include` and `#pragma`. There are no general macros (`#define` is not a feature).
]=]

--[=[
	Angle-bracket includes are IntelliSense (and `require` for libs). Quoted sibling stem is inlined.

	**Syntax:** `#include <clpp/roblox.clh>` · `#include "LeaderstatsServer.clh"`

	@within Preprocessor
]=]
function Preprocessor.include() end

--[=[
	Emit `--!strict` at the top of the Luau file.

	**Syntax:** `#pragma strict`

	**Emit:** `--!strict`

	@within Preprocessor
]=]
function Preprocessor.strict() end

--[=[
	Never emit `--!strict`, even if project config asks for it.

	**Syntax:** `#pragma nstrict`

	@within Preprocessor
]=]
function Preprocessor.nstrict() end

--[=[
	Parsed and ignored. Headers are not multiply included the C++ way.

	**Syntax:** `#pragma once`

	**Emit:** ignored

	@within Preprocessor
]=]
function Preprocessor.once() end

return Preprocessor
