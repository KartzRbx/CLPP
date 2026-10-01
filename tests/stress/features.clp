abstract struct Animal {
  private int secret;
  func reveal() { return self.secret; }
  abstract func speak() {}
}
struct Dog : Animal {
  int bones;
  override func speak() { return self.bones; }
}
struct Flyer { int lift; }
struct Runner { int pace; }
struct Hero : Flyer, Runner { int power; }
struct Box<T> { T item; }
enum Msg { Text(string), Num(int) }
func id<T>(T value) { return value; }
Dog pet = Dog(9, 4);
post(pet.reveal());
post(pet.speak());
Hero hero = Hero(3, 4, 5);
post(hero.lift);
post(hero.power);
Box<int> box = Box<int>(8);
post(box.item);
post(id<int>(3));
Msg note = Msg.Text("hi");
match (note) { Text(s) ~> post(s); Num(n) ~> post(n); }
Option<int> item = Some(6);
match (item) { Some(v) ~> post(v); None ~> post(0); }
Result<int, string> bad = Err("no");
match (bad) { Ok(v) ~> post(v); Err(e) ~> post(e); }
let x = 1;
if (1) { let x = 2; post(x); }
post(x);
