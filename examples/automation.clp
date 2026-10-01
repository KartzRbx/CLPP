<< OOP avançado: override, polimorfismo, herança múltipla, abstract, private e generics
link @clpp.axiom as Axiom;

struct Movable {
  Vector2 position;

  func label() -> string { return "unknown"; }
}

struct Unit : Movable {
  string name;
  private int hp;

  func revealHp() -> int { return @hp; }

  override func label() -> string { return @name; }

  func damage(int amount) {
    @hp = Axiom.Clamp(@hp - amount, 0, 100);
  }

  func distanceTo(Unit other) {
    return Axiom.Distance2(@position, other.position);
  }
}

struct Boss : Unit {
  int phase;

  override func label() -> string { return "BOSS " .: @name; }
}

abstract struct Shape {
  abstract func area() -> float {}
}

struct Square : Shape {
  float side;

  override func area() -> float { return @side * @side; }
}

struct Armored { int armor; }
struct Swift { int speed; }
struct Elite : Armored, Swift { string codename; }

struct Box<T> { T value; }

Movable scout = Unit(Vector2(0, 0), "Scout", 40);
Boss king = Boss(Vector2(10, 0), "King", 90, 2);

post(scout.label());
post(king.label());
post(king.revealHp());

Unit ally = Unit(Vector2(1, 5), "Ally", 50);
Unit foe = Unit(Vector2(2, 5), "Foe", 30);
post(ally.distanceTo(foe));

scout.position.x = scout.position.x + 3;
scout.position.y = scout.position.y + 4;
post(scout.position.x);

Square tile = Square(3);
post(tile.area());

Elite agent = Elite(12, 7, "Seven");
post(agent.armor .: "/" .: agent.speed .: " " .: agent.codename);

Box<int> tier = Box<int>(3);
post(tier.value);

king.damage(25);
post(king.revealHp());
