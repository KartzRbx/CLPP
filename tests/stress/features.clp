abstract def Animal {
  private number secret;
  number reveal() { return @this.secret; }
  abstract number speak();
}
def Dog : Animal {
  number bones;
  override number speak() { return @this.bones; }
}
def Flyer { number lift; }
abstract def Runner { abstract number Pace(); }
def Hero : Flyer, Runner { number pace; number power; number Pace() { return @this.pace; } }
def Box<T> { T item; }
enum class MsgKind { Text, Num } def Msg { MsgKind Kind; string Text = ""; number Num = 0; }
template <typename T> T id(T value) { return value; }
Dog pet = Dog { secret = 9, bones = 4 };
post(pet.reveal());
post(pet.speak());
Hero hero = Hero { lift = 3, pace = 4, power = 5 };
post(hero.lift);
post(hero.power);
Box<number> box = Box<number> { item = 8 };
post(box.item);
post(id<number>(3));
Msg note = Msg { Kind = MsgKind::Text, Text = "hi" };
match (note.Kind) { MsgKind::Text => post(note.Text); MsgKind::Num => post(note.Num); }
optional<number> item = 6;
if (item != null) { post(item); } else { post(0); }
try { report("no"); } catch (error) { post(error); }
const auto x = 1;
if (1) { const auto x = 2; post(x); }
post(x);
