--!strict
-- =====================================================================
-- Entities.expected.luau
--
-- PROPOSTA de emit para Entities.clh. Não é a saída atual do compilador:
-- é o contrato que a Fase 2 e a Fase 3 do prompt devem fixar em snapshot.
-- =====================================================================

-- def sem corpo (alias / union / função): só tipo, nada em runtime
export type Tile = "." | "o" | "#" | " "
export type Score = number
export type Predicate = (Vec2) -> boolean
export type ScoreListener = (Score, Vec2) -> ()

-- def com corpo: tipo exportado
export type Vec2 = { X: number, Y: number }
export type Grid<T> = { Width: number, Height: number, Cells: { T } }

-- herança vira intersecção; a tabela em runtime continua plana
export type Entity = { Pos: Vec2, Facing: Dir, StepMs: number }
export type Pacman = Entity & { Queued: Dir?, Lives: number, Points: Score }
export type Ghost = Entity & { Frightened: boolean }
export type Fruit = { Pos: Vec2, Points: Score, Active: boolean }

export type GameState = {
	Maze: Grid<Tile>,
	Player: Pacman,
	Ghosts: { Ghost },
	Bonus: Fruit,
	Remaining: number,
	PowerTimer: number,
	Won: boolean,
}

-- Valores padrão exigem construtor. Para campos booleanos o teste é
-- `== nil`, nunca `or`, porque `false or true` devolveria true.
local M = {}

function M.Vec2(init: { X: number?, Y: number? }): Vec2
	return {
		X = if init.X == nil then 0 else init.X,
		Y = if init.Y == nil then 0 else init.Y,
	}
end

function M.Fruit(init: { Pos: Vec2, Points: Score?, Active: boolean? }): Fruit
	return {
		Pos = init.Pos,
		Points = if init.Points == nil then 100 else init.Points,
		Active = if init.Active == nil then false else init.Active,
	}
end

return M

-- =====================================================================
-- Em um arquivo que faz `link "Entities.clh"`, o emit gera automaticamente:
--
--   local Entities = require(script.Parent.Entities)
--   type Vec2 = Entities.Vec2
--   type Tile = Entities.Tile
--   type GameState = Entities.GameState
--   ... (somente os tipos realmente usados no arquivo)
--
-- É isso que substitui `import type`: o Luau só enxerga tipos de outro
-- módulo via `Modulo.Tipo`, então o `link` cria os aliases locais.
-- =====================================================================
