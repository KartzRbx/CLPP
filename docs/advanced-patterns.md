---
title: Advanced production patterns
---

# Advanced production patterns

A compact combat pickup script: **guard**, **match**, **observable**, **signal**, **Once**, **spawn**, **[[server]]**.

```clpp
#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>

struct CombatServer {
    Janitor janitor;
    signal<Player, int> OnHit;
    void PlayerEntered(Player player);
    void BindPart(BasePart part);
};

void CombatServer::BindPart(BasePart part) {
    guard (part != null) else { return; }

    part.Touched~>Once(func (BasePart other) {
        Instance character = other.Parent;
        guard (character != null) else { return; }

        match (character.FindFirstChild("Humanoid")) {
            Humanoid humanoid => {
                guard (humanoid.Health > 0) else { return; }
                Player player = GetService<Players>().GetPlayerFromCharacter(character);
                guard (player != null) else { return; }
                OnHit.Fire(player, 10);
                part.Destroy();
            },
            _ => warn("touch without humanoid")
        };
    });
}

void CombatServer::PlayerEntered(Player player) {
    guard (player != null) else { return; }
    observable int combo = 0;

    OnHit~>Connect(func (Player victim, int amount) {
        guard (victim == player) else { return; }
        combo = combo + 1;
        post(player.Name .: " combo " .: combo .: " dmg " .: amount);
    });
}

[[server]]
void CombatServer::WatchWorkspace() {
    workspace.ChildAdded~>Connect(func (Instance child) {
        match (child) {
            BasePart p => BindPart(p),
            Model m => post("model spawned " .: m.Name),
            _ => {}
        };
    });
}

void init() {
    CombatServer combat;
    combat.janitor = new Janitor();

    spawn {
        task::wait(1);
        post("combat ready");
    };

    Players players = GetService<Players>();
    for (Player player in players.GetPlayers()) {
        combat.PlayerEntered(player);
    }
    players.PlayerAdded~>Connect(func (Player p) {
        combat.PlayerEntered(p);
    });
    combat.WatchWorkspace();
}
```

## Why this is “production-shaped”

- **guard** keeps every callback flat.
- **match** is the Instance type switch you would write with a pile of `IsA` in Luau.
- **Once** on `Touched` so a pickup cannot fire twice.
- **observable** combo is HUD-ready (`combo.OnChange` on the client).
- **signal.Fire** decouples damage from UI.
- **[[server]]** documents that workspace watching is not a LocalScript concern.
- **~>** means you do not leak connections when the janitor cleans up.

Full file: [`examples/advanced/CombatServer.server.clpp`](https://github.com/KartzRbx/CLPP/blob/main/examples/advanced/CombatServer.server.clpp).

Next: [Cheat sheet](cheatsheet).
