<< Métodos e herança: 300.000 chamadas virtuais
def Animal {
  number legs;
  number speed() { return @this.legs; }
}
def Dog : Animal {
  number bones;
  number speed() { return @this.legs + @this.bones; }
}
Dog d = Dog { legs = 4, bones = 2 };
Animal a = d;
auto total = 0;
for (auto i = 0; i < 300000; i += 1) {
  total += a.speed();
}
post(total);
