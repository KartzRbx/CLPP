<< Entities.clp — módulo de dados e callbacks com exports explícitos.


<< ---------------------------------------------------------------------
<< 1) def como registro de dados.
<<    Campos podem ter valor padrão.
<< ---------------------------------------------------------------------
export def Vec2 {
    number X = 0;
    number Y = 0;
}

<< ---------------------------------------------------------------------
<< 2) def como ALIAS / UNION
<<    Union de literais: o compilador sabe que Tile só pode ser um destes
<<    quatro valores; as demais strings são rejeitadas pelo compilador.
<< ---------------------------------------------------------------------
export def Tile = "." | "o" | "#" | " ";

export def Score = number;                          << alias simples

<< Enum com acesso aos membros por ::.
export enum class Dir { Up, Down, Left, Right }

<< ---------------------------------------------------------------------
<< 3) def como TIPO DE FUNÇÃO de primeira classe 
<<    Assinaturas verificadas para os callbacks.
<< ---------------------------------------------------------------------
export def Predicate     = (Vec2) -> bool;
export def ScoreListener = (Score, Vec2) -> void;

<< ---------------------------------------------------------------------
<< 4) def GENÉRICO 
<< ---------------------------------------------------------------------
export def Grid<T> {
    number Width  = 0;
    number Height = 0;
    array<T> Cells;
}

<< ---------------------------------------------------------------------
<< 5) def com HERANÇA nominal.
<< ---------------------------------------------------------------------
export def Entity {
    Vec2 Pos;
    Dir  Facing = Dir::Right;
    number  StepMs = 150;
}

export def Pacman : Entity {
    optional<Dir> Queued = null;                 << null = nenhum comando pendente
    number Lives = 3;
    Score Points = 0;
}

export def Ghost : Entity {
    bool Frightened = false;
}

export def Fruit {
    Vec2  Pos;
    Score Points = 100;
    bool  Active = false;
}

export def GameState {
    Grid<Tile>  Maze;
    Pacman      Player;
    array<Ghost> Ghosts;
    Fruit       Bonus;
    number         Remaining  = 0;
    float       PowerTimer = 0.0;
    bool        Won        = false;
}

<< ---------------------------------------------------------------------
