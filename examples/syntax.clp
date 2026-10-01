import @clpp.math as Math;
using Math.abs;
struct Point { int x; int y; }
signal ping;
static func show(int x) { post(x); }
func one() { return 1; }
auto base = 3;
observable n = 1;
n = n + 1;
constexpr doubled = 2 + 2;
cout(base);
warn(n);
try { report(doubled); } catch (e) { post(e); }
post(abs(0 - 5));
post(endl);
Vector2 v = Vector2(1, 2);
post(v.y);
post(Vector4(1, 2, 3, 4).w);
post(Vector2(8, 9)[1]);
post("AZ"[1]);
let mut sum = 0;
for (let i in 4) { sum = sum + i; }
post(sum);
post(pcall(1 / 0));
post(pcall(8 / 2));
ping ~> show;
ping(6);
post(one());
post(new Point(3, 4).x);
