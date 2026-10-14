<< Métodos: @this acessa a instância atual, inclusive para escrita e chamadas.
link @clpp.axiom as Axiom;

def Fighter {
  string name;
  number hp;
  number maxHp;
  Vector2 pos;

  bool isDown() { return @this.hp <= 0; }

  void applyDamage(number amount) {
    @this.hp = Axiom.Clamp(@this.hp - amount, 0, @this.maxHp);
  }

  void heal(number amount) {
    @this.hp = Axiom.Clamp(@this.hp + amount, 0, @this.maxHp);
  }

  string describe() {
    return @this.name .: " " .: @this.hp .: "/" .: @this.maxHp;
  }

  float distanceTo(Fighter other) {
    return Axiom.Distance2(@this.pos, other.pos);
  }

  void hurt(number amount) {
    @this.hp = Axiom.Clamp(@this.hp - amount, 0, @this.maxHp);
  }
}

Fighter hero = Fighter { name = "Ada", hp = 100, maxHp = 100, pos = Vector2(0, 0) };
Fighter target = Fighter { name = "Dummy", hp = 100, maxHp = 100, pos = Vector2(3, 4) };

post(hero.describe());
post(hero.distanceTo(target));

target.hurt(hero.hp * 15 / 100); << Dano proporcional em pontos inteiros.
post(target.describe());
post(target.isDown());
