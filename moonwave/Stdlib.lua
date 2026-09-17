local Stdlib = {}

--[=[
	@class Stdlib
	Headers under `stdlib/clpp`. Angle-bracket includes are IntelliSense (and `require` for libs).

	| Include | Effect | Page |
	| --- | --- | --- |
	| `<clpp/roblox.clh>` | engine globals, IntelliSense only | [roblox.clh](/CLPP/docs/reference/header-roblox) |
	| `<clpp/generated/instances.clh>` | Instance classes | [instances.clh](/CLPP/docs/reference/header-instances) |
	| `<clpp/datatypes.clh>` | Vector3, CFrame, `string_concat` | [datatypes.clh](/CLPP/docs/reference/header-datatypes) |
	| `<clpp/libs/janitor.clh>` | IntelliSense **and** `require` Janitor | [janitor.clh](/CLPP/docs/reference/header-janitor) |
	| `<clpp/libs/dataservice.clh>` | DataService | [dataservice.clh](/CLPP/docs/reference/header-dataservice) |

	Quoted includes: same stem as the `.clpp` is **inlined**; any other name becomes `require`.
]=]

--[=[
	`#include <path>` / `#include "path"`.

	**Emit:** `require(…)` for libs and non-stem quotes; stem quote is inlined; `roblox.clh` is IntelliSense only.

	@within Stdlib
]=]
function Stdlib.include() end

--[=[
	`#pragma strict` emits `--!strict`. `#pragma nstrict` never does. `#pragma once` is ignored.

	@within Stdlib
]=]
function Stdlib.pragma() end

return Stdlib
