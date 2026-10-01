<< Métodos e @campo (@hp = campo do objeto atual; @this.name em expressões)
link @clpp.axiom as Axiom;

struct Fighter {
  string name;
  int hp;
  int maxHp;
  Vector2 pos;

  func isDown() -> bool { return @hp <= 0; }

  func applyDamage(int amount) {
    @hp = Axiom.Clamp(@hp - amount, 0, @maxHp);
  }

  func heal(int amount) {
    @hp = Axiom.Clamp(@hp + amount, 0, @maxHp);
  }

  func describe() -> string {
    return @name .: " " .: @hp .: "/" .: @maxHp;
  }

  func distanceTo(Fighter other) {
    return Axiom.Distance2(@pos, other.pos);
  }

  func hurt(int amount) {
    @hp = Axiom.Clamp(@hp - amount, 0, @maxHp);
  }
}

Fighter hero = Fighter("Ada", 100, 100, Vector2(0, 0));
Fighter target = Fighter("Dummy", 100, 100, Vector2(3, 4));

post(hero.describe());
post(hero.distanceTo(target));

target.hurt(15);
post(target.describe());
post(target.isDown());
