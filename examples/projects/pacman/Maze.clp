<< Maze.clp — lógica procedural do mapa na VM CL++.
<< =====================================================================
<< =====================================================================



link "./Entities.clp" as E;

<< ---------------------------------------------------------------------
<< ---------------------------------------------------------------------
def Cell {
    E.Vec2 At;
    E.Tile Kind;
}

const number POWER_SECONDS = 8;              << const = único mecanismo de imutabilidade

<< Estado do módulo. Tudo é mutável por padrão (como em C++); não existe `mut`.
array<E.ScoreListener> listeners = array<E.ScoreListener>();

<< ---------------------------------------------------------------------
<< Grade genérica
<< ---------------------------------------------------------------------
template <typename T>
bool InBounds(E.Grid<T> grid, E.Vec2 at) {
    return at.X >= 0 && at.Y >= 0 && at.X < grid.Width && at.Y < grid.Height;
}

template <typename T>
number Index(E.Grid<T> grid, E.Vec2 at) {
    return at.Y * grid.Width + at.X;
}

export template <typename T>
optional<T> GridTryAt(E.Grid<T> grid, E.Vec2 at) {
    guard (InBounds(grid, at)) else { return null; }
    return grid.Cells[Index(grid, at)];
}

export template <typename T>
T GridAt(E.Grid<T> grid, E.Vec2 at) {
    optional<T> found = GridTryAt(grid, at);
    guard (found != null) else { post("GridAt fora do mapa"); return grid.Cells[0]; }
    return found;                         << OK: `found` foi estreitado de T? para T
}

<< ---------------------------------------------------------------------
<< Direções: match exaustivo sobre enum.
<< ---------------------------------------------------------------------
export E.Vec2 Delta(E.Dir dir) {
    match (dir) {
        E.Dir::Up    => { return E.Vec2 { X = 0,  Y = -1 }; }
        E.Dir::Down  => { return E.Vec2 { X = 0,  Y = 1 };  }
        E.Dir::Left  => { return E.Vec2 { X = -1, Y = 0 };  }
        E.Dir::Right => { return E.Vec2 { X = 1,  Y = 0 };  }
    }
}

export bool SamePos(E.Vec2 a, E.Vec2 b) {
    return a.X == b.X and a.Y == b.Y;     << `and` aceito; && também é aceito
}

<< Pure helper; effect annotations remain separate.
export number Manhattan(E.Vec2 a, E.Vec2 b) {
    return (a.X > b.X ? a.X - b.X : b.X - a.X)
         + (a.Y > b.Y ? a.Y - b.Y : b.Y - a.Y);
}

<< ---------------------------------------------------------------------
<< Regras do mapa
<< ---------------------------------------------------------------------
export bool IsWall(E.Grid<E.Tile> maze, E.Vec2 at) {
    optional<E.Tile> tile = GridTryAt(maze, at);
    guard (tile != null) else { return true; }    << fora do mapa conta como parede
    return tile == "#";
}

export E.Score Consume(E.Grid<E.Tile> maze, E.Vec2 at) {
    E.Tile current = GridAt(maze, at);
    E.Score gained = 0;

    match (current) {                     << E.Tile é union de 4 literais: todos obrigatórios
        "." => { gained = 10; }
        "o" => { gained = 50; }
        "#" => { gained = 0;  }
        " " => { gained = 0;  }
    }

    return gained;
}

<< ---------------------------------------------------------------------
<< Eventos: E.ScoreListener é um tipo de função checado de ponta a ponta
<< ---------------------------------------------------------------------
export void OnScore(E.ScoreListener listener) {
    push(listeners, listener);
}

void Notify(E.Score gained, E.Vec2 at) {
    for (E.ScoreListener listener in listeners) {
        listener(gained, at);
    }
}

<< ---------------------------------------------------------------------
<< Movimento. E.Pacman e E.Ghost passam como E.Entity por herança (IS-A).
<< `blocked` é um E.Predicate: assinatura errada é erro no ponto da chamada
<< Os callbacks recebem os tipos exportados pelo módulo de dados.
<< ---------------------------------------------------------------------
export void Move(E.Entity entity, E.Predicate blocked) {
    E.Vec2 d = Delta(entity.Facing);
    E.Vec2 next = E.Vec2 { X = entity.Pos.X + d.X, Y = entity.Pos.Y + d.Y };

    if (not blocked(next)) {              << `not` aceito; o fmt reescreve para !
        entity.Pos = next;
    }
}

export void Tick(E.GameState state, float dt) {
    E.Pacman p = state.Player;

    << aplica a direção enfileirada quando o caminho está livre
    optional<E.Dir> queued = p.Queued; if (queued != null) {                       << narrowing: dentro do if, Queued é E.Dir
        E.Dir wanted = queued;
        E.Vec2 d = Delta(wanted);
        E.Vec2 probe = E.Vec2 { X = p.Pos.X + d.X, Y = p.Pos.Y + d.Y };

        if (!IsWall(state.Maze, probe)) {
            p.Facing = wanted;
            p.Queued = null;
        }
    }

    << lambda tipada: closure captura `state` automaticamente (sem [&] de C++)
    Move(p, func (E.Vec2 at) -> bool { return IsWall(state.Maze, at); });

    E.Tile under = GridAt(state.Maze, p.Pos);

    if (under == "." or under == "o") {
        E.Score gained = Consume(state.Maze, p.Pos);
        p.Points += gained;
        state.Remaining -= 1;
        Notify(gained, p.Pos);

        if (under == "o") {
            state.PowerTimer = POWER_SECONDS;   << number -> float: alargamento permitido
        }
    }

    if (state.Bonus.Active and SamePos(p.Pos, state.Bonus.Pos)) {
        p.Points += state.Bonus.Points;
        state.Bonus.Active = false;
        Notify(state.Bonus.Points, p.Pos);
    }

    if (state.PowerTimer > 0.0) {
        state.PowerTimer -= dt;
    }

    state.Won = state.Remaining <= 0;
}
