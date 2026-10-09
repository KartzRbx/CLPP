local game = {GetService = function(self: any, name: string): any return {CluauppLibs = {Roster = {}, Hive = {}}} end}
local require = function(module: any): any return { new = function(): any return {} end, HiveWorld = {new = function(): any return {} end} } end
