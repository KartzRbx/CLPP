link @clpp.Axion as Axiom;
auto i = 0;
auto bad = 0;
auto sum = 0;
while (i < 150000) {
  auto x = i % 100;
  auto c = Axiom.Clamp(x, 0, 10);
  sum = sum + c;
  if (x <= 10) { if (c != x) { bad = bad + 1; } }
  if (x > 10) { if (c != 10) { bad = bad + 1; } }
  if (Axiom.Hypot(3, 4) != 5) { bad = bad + 1; }
  if (Axiom.Gcd(12, 8) != 4) { bad = bad + 1; }
  if (Axiom.Lerp(0, 10, 0) != 0) { bad = bad + 1; }
  i = i + 1;
}
post(bad);
post(sum);
post(Axiom.Hypot(3, 4));
post(Axiom.Gcd(12, 8));
