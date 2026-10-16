<< Métodos, @this, polimorfismo, uma base com armazenamento e interfaces sem campos.
link @clpp.axiom as Axiom;

def Movable {
  Vector2 position;

  string label() { return "unknown"; }
}

def Unit : Movable {
  string name;
  private number hp;

  number revealHp() { return @this.hp; }

  override string label() { return @this.name; }

  void damage(number amount) {
    @this.hp = Axiom.Clamp(@this.hp - amount, 0, 100);
  }

  float distanceTo(Unit other) {
    return Axiom.Distance2(@this.position, other.position);
  }
}

def Boss : Unit {
  number phase;

  override string label() { return "BOSS " .: @this.name; }
}

abstract def Shape {
  abstract float area();
}

def Square : Shape {
  float side;

  override float area() { return @this.side * @this.side; }
}

def Armored { number armor; }
abstract def Swift { abstract number Speed(); }
def Elite : Armored, Swift { number speed; string codename; number Speed() { return @this.speed; } }

def Box<T> { T value; }

Movable scout = Unit { position = Vector2(0, 0), name = "Scout", hp = 40 };
Boss king = Boss { position = Vector2(10, 0), name = "King", hp = 90, phase = 2 };

post(scout.label());
post(king.label());
post(king.revealHp());

Unit ally = Unit { position = Vector2(1, 5), name = "Ally", hp = 50 };
Unit foe = Unit { position = Vector2(2, 5), name = "Foe", hp = 30 };
post(ally.distanceTo(foe));

scout.position.x = scout.position.x + 3;
scout.position.y = scout.position.y + 4;
post(scout.position.x);

Square tile = Square { side = 3 };
post(tile.area());

Elite agent = Elite { armor = 12, speed = 7, codename = "Seven" };
post(agent.armor .: "/" .: agent.speed .: " " .: agent.codename);

Box<number> tier = Box<number> { value = 3 };
post(tier.value);

king.damage(25);
post(king.revealHp());
