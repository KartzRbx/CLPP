<< Métodos e herança: 300.000 chamadas virtuais
struct Animal {
  int legs;
  func speed() { return self.legs; }
}
struct Dog : Animal {
  int bones;
  func speed() { return self.legs + self.bones; }
}
Dog d = Dog(4, 2);
Animal a = d;
let mut total = 0;
for (let mut i = 0; i < 300000; i += 1) {
  total += a.speed();
}
post(total);
