<< 5) def com HERANÇA nominal.
<<[ Both comment forms must remain entirely comments, including def and return. ]>>
def Outcome = "ongoing" | "team_a" | "team_b";
const auto PANEL = 0x141025;
const auto PORTRAIT = 0x2D1B4E;
const auto HP = 0xFF5A8A;
const auto MP = 0x5EC8FF;

def Fighter {
  string Name;
  number Hp;
  bool IsDown() { return @this.Hp <= 0; }
  string Describe() { return @this.Name .: ": " .: @this.Hp; }
}

Outcome RunBattle(Fighter fighter) {
  return fighter.IsDown() ? "team_b" : "ongoing";
}

auto hero = Fighter { Name = "Ada", Hp = 100 };
post(hero.Describe());
post(RunBattle(hero));
