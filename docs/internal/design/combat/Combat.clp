// =====================================================================
// Combat.clp  —  sistema de combate "Ressonância"
//
// Regras inventadas:
//  - Cada habilidade tem um elemento. Usar elementos DIFERENTES em
//    sequência acumula Ressonância (até 3), +20% de dano por acúmulo.
//  - Repetir o mesmo elemento zera a Ressonância e causa Fadiga (-30% dano).
//  - Fogo > Vento > Terra > Água > Fogo (1.5x); o inverso é 0.75x. Vazio é neutro.
//  - Efeitos ao acertar: Fogo queima, Água encharca (-3 de velocidade e
//    Vento causa +30%), Terra cria escudo. Vazio atordoa, mas só com
//    Ressonância máxima, e consome os acúmulos.
//
// Legenda:
//   [NOVO]       recurso proposto (def, tipos de função, alias genérico)
//   [VERIFICAR]  forma deduzida da documentação, confirmar no grammar.pest
// =====================================================================

pragma strict;

// ---------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------
enum class Element    { Fire, Water, Earth, Wind, Void }
enum class StatusKind { Burn, Soak, Stun, Shield, Fatigue }

def Outcome = "ongoing" | "team_a" | "team_b";      // [NOVO] union de literais

def Stats {                                          // [NOVO] def com padrões
    int MaxHp = 100;
    int Hp    = 100;
    int Atk   = 10;
    int Def   = 5;
    int Speed = 10;
}

def Status {
    StatusKind Kind;
    int Turns;
    int Power = 0;                                   // dano da queimadura / vida do escudo
}

def Skill {
    string  Name;
    Element Affinity;
    int     Power;
    int     Cost     = 0;
    int     Cooldown = 0;
}

def Slot {                                           // habilidade equipada + recarga
    Skill Skill;
    int   Left = 0;
}

def Fighter {
    string  Name;
    Element Attunement = Element::Void;              // elemento do próprio lutador
    Stats   Vitals;
    array<Slot>   Slots    = {};                     // [VERIFICAR] literal de array vazio
    array<Status> Statuses = {};
    optional<Element> LastElement;                   // null = ainda não atacou
    int Resonance = 0;
    int Mana      = 30;
}

def Battle {
    array<Fighter> TeamA;
    array<Fighter> TeamB;
    int     Round  = 1;
    Outcome Result = "ongoing";
}

def DamageCtx {                                      // contexto que o pipeline vai alterando
    Fighter Attacker;
    Fighter Target;
    Skill   Skill;
    float   Amount = 0.0;
    bool    Crit   = false;
}

def Action { Slot Slot; Fighter Target; }

// Tipos de função de primeira classe  [NOVO]
def Rng      = () -> float;
def Logger   = (string) -> void;
def Modifier = (DamageCtx) -> void;
def Brain    = (Fighter, Battle) -> optional<Action>;
def Less<T>  = (T, T) -> bool;                       // alias genérico de função

const float CRIT_CHANCE      = 0.15;
const float CRIT_MULT        = 1.75;
const float RESONANCE_BONUS  = 0.20;
const int   MAX_RESONANCE    = 3;
const int   MANA_REGEN       = 5;
const int   MAX_MANA         = 30;
const int   MAX_ROUNDS       = 50;

// ---------------------------------------------------------------------
// Elementos (match exaustivo: esquecer um caso é CLPP0501)
// ---------------------------------------------------------------------
Element Beats(Element e) {
    match (e) {
        Element::Fire  => { return Element::Wind;  }
        Element::Wind  => { return Element::Earth; }
        Element::Earth => { return Element::Water; }
        Element::Water => { return Element::Fire;  }
        Element::Void  => { return Element::Void;  }
    }
}

float AffinityMultiplier(Element attack, Element target) {
    if (attack == Element::Void or target == Element::Void) { return 1.0; }
    if (Beats(attack) == target) { return 1.5;  }
    if (Beats(target) == attack) { return 0.75; }
    return 1.0;
}

// ---------------------------------------------------------------------
// Estados
// ---------------------------------------------------------------------
optional<Status> FindStatus(Fighter f, StatusKind kind) {
    for (Status s in f.Statuses) {
        if (s.Kind == kind) { return s; }
    }
    return null;
}

bool HasStatus(Fighter f, StatusKind kind) {
    return FindStatus(f, kind) != null;
}

void AddStatus(Fighter f, StatusKind kind, int turns, int power) {
    optional<Status> existing = FindStatus(f, kind);

    if (existing) {                                  // narrowing: aqui `existing` é Status
        existing.Turns = turns > existing.Turns ? turns : existing.Turns;
        existing.Power = power > existing.Power ? power : existing.Power;
        return;
    }

    f.Statuses.push(Status { .Kind = kind, .Turns = turns, .Power = power });
}

bool IsAlive(Fighter f) { return f.Vitals.Hp > 0; }

int EffectiveSpeed(Fighter f) {
    return HasStatus(f, StatusKind::Soak) ? f.Vitals.Speed - 3 : f.Vitals.Speed;
}

// ---------------------------------------------------------------------
// Pipeline de dano: lista de Modifier executados em ordem.
// Cada etapa é um tipo de função checado, e `rng` é injetado.
// Isso deixa a batalha determinística nos testes.
// ---------------------------------------------------------------------
array<Modifier> BuildPipeline(Rng rng) {
    array<Modifier> pipeline = {};

    // 1) dano base
    pipeline.push(func (DamageCtx c) -> void {
        float raw = static_cast<float>(
            c.Skill.Power + c.Attacker.Vitals.Atk - c.Target.Vitals.Def);
        c.Amount = raw > 1.0 ? raw : 1.0;
    });

    // 2) afinidade elemental contra o elemento do alvo
    pipeline.push(func (DamageCtx c) -> void {
        c.Amount *= AffinityMultiplier(c.Skill.Affinity, c.Target.Attunement);
    });

    // 3) Ressonância acumulada
    pipeline.push(func (DamageCtx c) -> void {
        c.Amount *= 1.0 + RESONANCE_BONUS * static_cast<float>(c.Attacker.Resonance);
    });

    // 4) alvo encharcado sofre mais de Vento
    pipeline.push(func (DamageCtx c) -> void {
        if (HasStatus(c.Target, StatusKind::Soak) and c.Skill.Affinity == Element::Wind) {
            c.Amount *= 1.3;
        }
    });

    // 5) atacante fatigado
    pipeline.push(func (DamageCtx c) -> void {
        if (HasStatus(c.Attacker, StatusKind::Fatigue)) { c.Amount *= 0.7; }
    });

    // 6) crítico
    pipeline.push(func (DamageCtx c) -> void {
        if (rng() < CRIT_CHANCE) {
            c.Amount *= CRIT_MULT;
            c.Crit = true;
        }
    });

    // 7) escudo absorve o que sobrar
    pipeline.push(func (DamageCtx c) -> void {
        optional<Status> shield = FindStatus(c.Target, StatusKind::Shield);
        guard (shield) else { return; }

        float cap = static_cast<float>(shield.Power);
        float absorbed = c.Amount < cap ? c.Amount : cap;
        shield.Power -= static_cast<int>(absorbed);
        c.Amount -= absorbed;
    });

    return pipeline;
}

DamageCtx ResolveDamage(Fighter attacker, Fighter target, Skill skill,
                        array<Modifier> pipeline) {
    DamageCtx ctx = DamageCtx { .Attacker = attacker, .Target = target, .Skill = skill };
    for (Modifier step in pipeline) { step(ctx); }
    return ctx;
}

// Estimativa pura para a interface (sem crit, sem efeitos). Checada por @parallel:
// qualquer atribuição aqui dentro seria CLUAU_PAR001.
@parallel
int PreviewDamage(Stats attacker, Stats target, int power) {
    return (power + attacker.Atk - target.Def) > 1
         ? (power + attacker.Atk - target.Def)
         : 1;
}

// ---------------------------------------------------------------------
// Uso de habilidade
// ---------------------------------------------------------------------
bool CanUse(Fighter f, Slot slot) {
    return IsAlive(f)
       and not HasStatus(f, StatusKind::Stun)
       and f.Mana >= slot.Skill.Cost
       and slot.Left <= 0;
}

void UpdateResonance(Fighter user, Element used, Logger log) {
    optional<Element> last = user.LastElement;
    guard (last) else { user.LastElement = used; return; }

    if (last == used) {                              // repetiu: perde tudo e fadiga
        user.Resonance = 0;
        AddStatus(user, StatusKind::Fatigue, 2, 0);
        log(user.Name .: " repetiu o elemento e ficou fatigado");
    } else {
        user.Resonance = user.Resonance < MAX_RESONANCE
                       ? user.Resonance + 1
                       : MAX_RESONANCE;
    }
    user.LastElement = used;
}

void ApplyOnHit(Fighter user, Fighter target, Element affinity) {
    match (affinity) {
        Element::Fire  => { AddStatus(target, StatusKind::Burn, 3, 4); }
        Element::Water => { AddStatus(target, StatusKind::Soak, 2, 0); }
        Element::Earth => { AddStatus(user, StatusKind::Shield, 2, 12); }
        Element::Wind  => { }
        Element::Void  => {
            if (user.Resonance >= MAX_RESONANCE) {
                AddStatus(target, StatusKind::Stun, 1, 0);
                user.Resonance = 0;
            }
        }
    }
}

bool UseSkill(Fighter user, Slot slot, Fighter target,
              array<Modifier> pipeline, Logger log) {
    guard (CanUse(user, slot)) else { return false; }

    user.Mana -= slot.Skill.Cost;
    slot.Left = slot.Skill.Cooldown;
    UpdateResonance(user, slot.Skill.Affinity, log);

    DamageCtx ctx = ResolveDamage(user, target, slot.Skill, pipeline);
    int dealt = static_cast<int>(ctx.Amount);
    int after = target.Vitals.Hp - dealt;
    target.Vitals.Hp = after > 0 ? after : 0;

    ApplyOnHit(user, target, slot.Skill.Affinity);

    log(user.Name .: " usa " .: slot.Skill.Name .: " em " .: target.Name
        .: " e causa " .: to_string(dealt)
        .: (ctx.Crit ? " (CRÍTICO)" : "")
        .: " [" .: to_string(target.Vitals.Hp) .: " PV]");
    return true;
}

// ---------------------------------------------------------------------
// Início de turno: efeitos, duração e recarga
// ---------------------------------------------------------------------
void StartTurn(Fighter f, Logger log) {
    array<Status> kept = {};

    for (Status s in f.Statuses) {
        match (s.Kind) {
            StatusKind::Burn => {
                int after = f.Vitals.Hp - s.Power;
                f.Vitals.Hp = after > 0 ? after : 0;
                log(f.Name .: " sofre " .: to_string(s.Power) .: " de queimadura");
            }
            StatusKind::Soak    => { }
            StatusKind::Stun    => { }
            StatusKind::Shield  => { }
            StatusKind::Fatigue => { }
        }

        s.Turns -= 1;
        if (s.Turns > 0) { kept.push(s); }
    }
    f.Statuses = kept;

    for (Slot slot in f.Slots) {
        if (slot.Left > 0) { slot.Left -= 1; }
    }

    int mana = f.Mana + MANA_REGEN;
    f.Mana = mana < MAX_MANA ? mana : MAX_MANA;
}

// ---------------------------------------------------------------------
// Ordenação genérica com comparador tipado
// ---------------------------------------------------------------------
template <typename T>
void SortBy(array<T> items, Less<T> before) {
    for (int i = 1; i < items.size(); i++) {         // [VERIFICAR] .size()
        T current = items[i];
        int j = i - 1;
        while (j >= 0 and before(current, items[j])) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = current;
    }
}

array<Fighter> TurnOrder(Battle battle) {
    array<Fighter> all = {};
    for (Fighter f in battle.TeamA) { if (IsAlive(f)) { all.push(f); } }
    for (Fighter f in battle.TeamB) { if (IsAlive(f)) { all.push(f); } }

    SortBy(all, func (Fighter a, Fighter b) -> bool {
        return EffectiveSpeed(a) > EffectiveSpeed(b);
    });
    return all;
}

// ---------------------------------------------------------------------
// IA padrão: ataca o inimigo mais fraco com a melhor habilidade,
// preferindo um elemento diferente do último (para manter a Ressonância).
// ---------------------------------------------------------------------
bool Contains(array<Fighter> team, Fighter f) {
    for (Fighter other in team) {
        if (other == f) { return true; }             // referência: é o que queremos aqui
    }
    return false;
}

optional<Fighter> Weakest(array<Fighter> team) {
    optional<Fighter> best = null;
    for (Fighter f in team) {
        if (!IsAlive(f)) { continue; }
        if (!best) { best = f; continue; }
        if (f.Vitals.Hp < best.Vitals.Hp) { best = f; }
    }
    return best;
}

int PickScore(Fighter self, Slot slot) {
    optional<Element> last = self.LastElement;
    bool alternates = last == null or last != slot.Skill.Affinity;
    return slot.Skill.Power + (alternates ? 10 : 0);
}

optional<Action> DefaultBrain(Fighter self, Battle battle) {
    array<Fighter> enemies = Contains(battle.TeamA, self) ? battle.TeamB : battle.TeamA;

    optional<Fighter> target = Weakest(enemies);
    guard (target) else { return null; }

    optional<Slot> pick = null;
    for (Slot slot in self.Slots) {
        if (!CanUse(self, slot)) { continue; }
        if (!pick) { pick = slot; continue; }
        if (PickScore(self, slot) > PickScore(self, pick)) { pick = slot; }
    }
    guard (pick) else { return null; }

    return Action { .Slot = pick, .Target = target };
}

// ---------------------------------------------------------------------
// Rodadas e fim de batalha
// ---------------------------------------------------------------------
bool AnyAlive(array<Fighter> team) {
    for (Fighter f in team) { if (IsAlive(f)) { return true; } }
    return false;
}

Outcome CheckOutcome(Battle battle) {
    bool a = AnyAlive(battle.TeamA);
    bool b = AnyAlive(battle.TeamB);
    if (a and b) { return "ongoing"; }
    return a ? "team_a" : "team_b";
}

void RunRound(Battle battle, Brain brain, array<Modifier> pipeline, Logger log) {
    log("— Rodada " .: to_string(battle.Round) .: " —");

    for (Fighter f in TurnOrder(battle)) {
        if (battle.Result != "ongoing") { break; }
        if (!IsAlive(f)) { continue; }

        bool stunned = HasStatus(f, StatusKind::Stun);   // checa ANTES de decrementar
        StartTurn(f, log);

        if (!IsAlive(f)) {
            log(f.Name .: " caiu pelas queimaduras");
            battle.Result = CheckOutcome(battle);
            continue;
        }
        if (stunned) {
            log(f.Name .: " está atordoado e perde o turno");
            continue;
        }

        optional<Action> action = brain(f, battle);
        guard (action) else { continue; }

        UseSkill(f, action.Slot, action.Target, pipeline, log);
        battle.Result = CheckOutcome(battle);
    }

    battle.Round += 1;
}

Outcome RunBattle(Battle battle, Brain brain, Rng rng, Logger log) {
    array<Modifier> pipeline = BuildPipeline(rng);

    while (battle.Result == "ongoing" and battle.Round <= MAX_ROUNDS) {
        RunRound(battle, brain, pipeline, log);
    }
    return battle.Result;
}

// ---------------------------------------------------------------------
// Demonstração: batalha 2x2 totalmente determinística
// ---------------------------------------------------------------------
Outcome Demo(Logger log) {
    int seed = 7;                                    // Park–Miller: fica abaixo de 2^53
    Rng rng = func () -> float {
        seed = (seed * 16807) % 2147483647;
        return static_cast<float>(seed) / 2147483647.0;
    };

    Skill ember  = Skill { .Name = "Brasa",     .Affinity = Element::Fire,  .Power = 14, .Cost = 6 };
    Skill tide   = Skill { .Name = "Maré",      .Affinity = Element::Water, .Power = 12, .Cost = 5 };
    Skill gust   = Skill { .Name = "Rajada",    .Affinity = Element::Wind,  .Power = 13, .Cost = 4 };
    Skill quake  = Skill { .Name = "Tremor",    .Affinity = Element::Earth, .Power = 11, .Cost = 5, .Cooldown = 1 };
    Skill rift   = Skill { .Name = "Fenda",     .Affinity = Element::Void,  .Power = 18, .Cost = 9, .Cooldown = 2 };

    Fighter ayla = Fighter {
        .Name = "Ayla", .Attunement = Element::Fire,
        .Vitals = Stats { .MaxHp = 90, .Hp = 90, .Atk = 12, .Speed = 14 },
        .Slots = { Slot { .Skill = ember }, Slot { .Skill = gust }, Slot { .Skill = rift } }
    };
    Fighter brom = Fighter {
        .Name = "Brom", .Attunement = Element::Earth,
        .Vitals = Stats { .MaxHp = 130, .Hp = 130, .Def = 8, .Speed = 7 },
        .Slots = { Slot { .Skill = quake }, Slot { .Skill = tide } }
    };
    Fighter nix = Fighter {
        .Name = "Nix", .Attunement = Element::Void,
        .Vitals = Stats { .MaxHp = 80, .Hp = 80, .Atk = 14, .Speed = 11 },
        .Slots = { Slot { .Skill = rift }, Slot { .Skill = gust }, Slot { .Skill = ember } }
    };
    Fighter lume = Fighter {
        .Name = "Lume", .Attunement = Element::Water,
        .Vitals = Stats { .MaxHp = 100, .Hp = 100, .Speed = 9 },
        .Slots = { Slot { .Skill = tide }, Slot { .Skill = quake } }
    };

    Battle battle = Battle { .TeamA = { ayla, brom }, .TeamB = { nix, lume } };

    Outcome result = RunBattle(battle, DefaultBrain, rng, log);   // função passada como valor
    log("Resultado: " .: result);
    return result;
}