# examples/

Sample CL++ programs. Instance methods use `.` (`hello.Greet(player)`). Definitions stay `Class::Method`. Inside a method, `@this` / `@field` is `self`.

| Folder | Files | Shows |
| --- | --- | --- |
| `hello/` | `hello.server.clpp` | `struct` + `Class::Method`, `.Greet`, range-for, `~>Connect`, `func (` |
| `syntax/` | `features.clp` | Types, `@coins`, `observable`, `signal`, `guard`, `~>`, `async`/`await`, `match`, `spawn` |
| `shared/` | `config.clp`, `PlayerData.clh` | Constants module and data Template |
| `ui/` | `FusionHud.client.clpp`, `VideCounter.client.clpp` | Declarative UI mounted with `.Mount` |
| `advanced/` | `CombatServer.server.clpp` | `@janitor`, `@OnHit`, `@this.BindPart`, `guard`, `match`, `[[server]]` |
| `leaderstats/` | `LeaderstatsServer.clh` + `.server.clpp` | Header + `@this` / `@janitor`, DataService, `~>Connect` |

Extensions: `.clh` (header), `.clp` (module), `.clpp` (implementation / script).
