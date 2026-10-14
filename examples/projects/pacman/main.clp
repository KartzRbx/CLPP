link "./Entities.clp" as E;
link "./Maze.clp" as Maze;

auto grid = E.Grid<E.Tile> { Width = 3, Height = 1, Cells = array<E.Tile>(".", "o", " ") };
auto player = E.Pacman { Pos = E.Vec2 { X = 0, Y = 0 }, Facing = E.Dir::Right };
auto state = E.GameState {
  Maze = grid, Player = player, Ghosts = array<E.Ghost>(),
  Bonus = E.Fruit { Pos = E.Vec2 { X = 2, Y = 0 }, Active = true }, Remaining = 1
};
Maze.OnScore(func(points, at) -> void { post(points); post(at.X); });
Maze.Tick(state, 0.25);
post(state.Player.Points); post(state.Won); post(state.PowerTimer);
Maze.Tick(state, 0.25);
post(state.Player.Points); post(state.Bonus.Active); post(state.Player.Pos.X);
Maze.Tick(state, 0.25);
post(state.Player.Pos.X); post(Maze.GridTryAt(grid, E.Vec2 { X = 9 }) == null);
