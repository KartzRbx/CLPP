---
title: Emit mapping
---

# Emit mapping

Fixed order of generated Luau:

1. `--!strict` if `#pragma strict`, or `--!nonstrict` if `#pragma nostrict`
2. `--!native` if `#pragma native`
3. `--!optimize N` if `#pragma optimize` / `#pragma optimize N`
4. `game:GetService(...)`
5. `require(...)` (libs and quoted includes that are not the stem)
6. types (`export type`)
7. constants
8. functions / script body
9. `init()` if present (Script / LocalScript)

Formatting is StyLua's job; Luau type checking is `luau-analyze`'s job.

## Operators

| CL++ | Luau |
| --- | --- |
| `::` on a call | `:` |
| `::` without a call (scope) | `.` |
| `:` (table / dictionary) | `.` |
| `.` (property) | `.` |
| `.:` | `..` |
| `null` | `nil` |
| `post` | `print` |
| `report` | `error` |
| `warn` | `warn` |
| `++` / `--` | `+= 1` / `-= 1` |
| `~>` | `janitor:Add(..., "Disconnect")` |

## Patterns used in leaderstats

| CL++ | Luau |
| --- | --- |
| `string GetPlayerJanitorKey(Player* player)` | `const function GetPlayerJanitorKey(player: Player): string` |
| `player.Name .: "_LeaderstatsJanitor"` | `player.Name .. "_LeaderstatsJanitor"` |
| `currentValue.Value != newValue` | `currentValue.Value ~= newValue` |
| `player::FindFirstChild("leaderstats")` | `player:FindFirstChild("leaderstats")` |
| `static_cast<Folder*>(existingFolder)` | `existingFolder` (annotated `Folder`) |
| `new Folder(player)` | `Instance.new("Folder"); folder.Parent = player` |
| `new IntValue(newFolder)` | `Instance.new("IntValue"); coins.Parent = newFolder` |
| `GetService<Players>()` | `game:GetService("Players")` |
| `DataService:Server::WaitFor(player)` | `DataService.Server:WaitFor(player)` |
| `signal::Connect(func [](int n) { ... })` | `signal:Connect(function(n: number) ... end)` |
| `for (Player* player : players::GetPlayers())` | `for _, player in players:GetPlayers() do` |
| `game::BindToClose(func []() { ... })` | `game:BindToClose(function() ... end)` |
| `null` / `if (existingFolder)` | `nil` / truthy |
| `observable int coins = 100` | `Instance.new("IntValue"); coins.Value = 100` |
| `coins.OnChange(fn)` | `coins.Changed:Connect(fn)` |
| `signal~>Connect(fn)` | `janitor:Add(signal:Connect(fn), "Disconnect")` |
| `signal~>Once(fn)` | `janitor:Add(signal:Once(fn), "Disconnect")` |
| `signal::Fire(...)` | `signal:Fire(...)` |
| `obj::GetPropertyChangedSignal("Name")` | `obj:GetPropertyChangedSignal("Name")` |
| `guard (x) else { return; }` | `if not (x) then return end` |
| `await expr` | `__await(expr)` |
| `auto [a, b] = fn();` | `local a, b = fn()` |
| `spawn { ... };` | `task.spawn(function() ... end)` |
| `parallel { ... };` | `task.desynchronize()` / `task.synchronize()` |
