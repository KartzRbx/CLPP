number inc(number x) { return x + 1; }
any first(any ... values) { return values[0]; }
template <typename T> T id(T value) where T: number { return value; }
number DemoPing() { return 4; }
post(0 ? 2 : 3);
post(1 && 0);
post(0 || 4);
post(6 & 3);
post(1 | 2);
post(1 shl 3);
post(~0);
auto n = 1;
n++;
post(n);
post("abcd"[1 .. 3]);
post(inc(1));
post(inc(4));
post(first(7, 8));
post((func(number x) -> number { return x + 1; })(2));
array<number> xs = array<number>(1, 2, 3);
post(xs[1]);
dictionary<string, number> sheet = dictionary<string, number>("a", 9);
post(sheet["a"]);
number y = 5; if (y != 0) { post(y); }
auto pair = list(8, 9); auto a = pair[0]; auto b = pair[1];
post(a);
post(b);
post(DemoPing());
post(id<number>(3));
try { post(1 / 0); } catch (e) { post(0); }

post(1);
