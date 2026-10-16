// =====================================================================
// Maze.clp  —  implementação do módulo (ModuleScript)
// =====================================================================

pragma strict;

link "Entities.clh";                      // [VERIFICAR] sintaxe exata do link

// ---------------------------------------------------------------------
// def PRIVADO: declarado fora de .clh, só existe dentro deste módulo.
// Outro arquivo que tentar usar `Cell` recebe CLPP0904 (ver
// Diagnostics.expected.clpp).  [NOVO]
// ---------------------------------------------------------------------
def Cell {
    Vec2 At;
    Tile Kind;
}

const int POWER_SECONDS = 8;              // const = único mecanismo de imutabilidade

// Estado do módulo. Tudo é mutável por padrão (como em C++); não existe `mut`.
array<ScoreListener> listeners;

// ---------------------------------------------------------------------
// Grade genérica
// ---------------------------------------------------------------------
template <typename T>
bool InBounds(Grid<T> grid, Vec2 at) {
    return at.X >= 0 && at.Y >= 0 && at.X < grid.Width && at.Y < grid.Height;
}

template <typename T>
int Index(Grid<T> grid, Vec2 at) {
    // o emit cuida do deslocamento para arrays 1-based do Luau [VERIFICAR]
    return at.Y * grid.Width + at.X;
}

template <typename T>
optional<T> GridTryAt(Grid<T> grid, Vec2 at) {
    guard (InBounds(grid, at)) else { return null; }
    return grid.Cells[Index(grid, at)];
}

template <typename T>
T GridAt(Grid<T> grid, Vec2 at) {
    optional<T> found = GridTryAt(grid, at);
    guard (found) else { post("GridAt fora do mapa"); return grid.Cells[0]; }
    return found;                         // OK: `found` foi estreitado de T? para T
}

// ---------------------------------------------------------------------
// Direções: match EXAUSTIVO sobre enum. Esquecer um caso é CLPP0501.
// ---------------------------------------------------------------------
Vec2 Delta(Dir dir) {
    match (dir) {
        Dir::Up    => { return Vec2 { .X = 0,  .Y = -1 }; }   // [VERIFICAR] .Campo = valor
        Dir::Down  => { return Vec2 { .X = 0,  .Y = 1 };  }
        Dir::Left  => { return Vec2 { .X = -1, .Y = 0 };  }
        Dir::Right => { return Vec2 { .X = 1,  .Y = 0 };  }
    }
}

bool SamePos(Vec2 a, Vec2 b) {
    return a.X == b.X and a.Y == b.Y;     // `and` aceito; o fmt reescreve para &&
}

@parallel
int Manhattan(Vec2 a, Vec2 b) {
    // sem atribuições aqui dentro (CLUAU_PAR001); só leitura e retorno
    return (a.X > b.X ? a.X - b.X : b.X - a.X)
         + (a.Y > b.Y ? a.Y - b.Y : b.Y - a.Y);
}

// ---------------------------------------------------------------------
// Regras do mapa
// ---------------------------------------------------------------------
bool IsWall(Grid<Tile> maze, Vec2 at) {
    optional<Tile> tile = GridTryAt(maze, at);
    guard (tile) else { return true; }    // fora do mapa conta como parede
    return tile == "#";
}

Score Consume(Grid<Tile> maze, Vec2 at) {
    Tile current = GridAt(maze, at);
    Score gained = 0;

    match (current) {                     // Tile é union de 4 literais: todos obrigatórios
        "." => { gained = 10; }
        "o" => { gained = 50; }
        "#" => { gained = 0;  }
        " " => { gained = 0;  }
    }

    maze.Cells[Index(maze, at)] = " ";    // def com corpo é REFERÊNCIA (tabela Luau)
    return gained;
}

// ---------------------------------------------------------------------
// Eventos: ScoreListener é um tipo de função checado de ponta a ponta
// ---------------------------------------------------------------------
void OnScore(ScoreListener listener) {
    listeners.push(listener);
}

void Notify(Score gained, Vec2 at) {
    for (ScoreListener listener in listeners) {
        listener(gained, at);
    }
}

// ---------------------------------------------------------------------
// Movimento. Pacman e Ghost passam como Entity por herança (IS-A).
// `blocked` é um Predicate: assinatura errada é erro no ponto da chamada
// (CLPP0905).
// ---------------------------------------------------------------------
void Move(Entity entity, Predicate blocked) {
    Vec2 d = Delta(entity.Facing);
    Vec2 next = Vec2 { .X = entity.Pos.X + d.X, .Y = entity.Pos.Y + d.Y };

    if (not blocked(next)) {              // `not` aceito; o fmt reescreve para !
        entity.Pos = next;
    }
}

void Tick(GameState state, float dt) {
    Pacman p = state.Player;

    // aplica a direção enfileirada quando o caminho está livre
    if (p.Queued) {                       // narrowing: dentro do if, Queued é Dir
        Dir wanted = p.Queued;
        Vec2 d = Delta(wanted);
        Vec2 probe = Vec2 { .X = p.Pos.X + d.X, .Y = p.Pos.Y + d.Y };

        if (!IsWall(state.Maze, probe)) {
            p.Facing = wanted;
            p.Queued = null;
        }
    }

    // lambda tipada: closure captura `state` automaticamente (sem [&] de C++)
    Move(p, func (Vec2 at) -> bool { return IsWall(state.Maze, at); });

    Tile under = GridAt(state.Maze, p.Pos);

    if (under == "." or under == "o") {
        Score gained = Consume(state.Maze, p.Pos);
        p.Points += gained;
        state.Remaining -= 1;
        Notify(gained, p.Pos);

        if (under == "o") {
            state.PowerTimer = POWER_SECONDS;   // int -> float: alargamento permitido
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
