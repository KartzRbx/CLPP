func inc(int x = 1) -> int { return x + 1; }
func first(... values) { return values[0]; }
func id<T>(T value) where T: int { return value; }
namespace Demo { func ping() { return 4; } }
post(0 ? 2 : 3);
post(1 && 0);
post(0 || 4);
post(6 & 3);
post(1 | 2);
post(1 shl 3);
post(~0);
let mut n = 1;
n++;
post(n);
post("abcd"[1 .. 3]);
post(inc());
post(inc(x: 4));
post(first(7, 8));
post((x => x + 1)(2));
array<int> xs = array<int>(1, 2, 3);
post(xs[1]);
dictionary<string, int> sheet = dictionary<string, int>("a", 9);
post(sheet["a"]);
if (let y = 5) { post(y); }
let (a, b) = list(8, 9);
post(a);
post(b);
post(Demo.ping());
post(id<int>(3));
try { post(1 / 0); } catch (e) { post(0); }
[[server]]
post(1);
