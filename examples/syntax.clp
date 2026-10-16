<< Sintaxe atual: funções tipadas, def, eventos e observáveis.
link @clpp.math as Math;
def Point { number x; number y; }
signal<number> ping;
void Show(number value) { post(value); }
number One() { return 1; }
constexpr number doubled = 2 + 2;
post(doubled);
observable number count = 1;
auto observation = count.OnChange(func(value) -> void { post(value); });
count += 1;
observation::Disconnect();
post(Math.abs(-5));
Vector2 vector = Vector2(1, 2);
post(vector.y);
post(Vector4(1, 2, 3, 4).w);
post("AZ"[1]);
number sum = 0;
for (number value in 4) { sum += value; }
post(sum);
try { report("example error"); } catch (error) { post(error); }
auto subscription = ping~>Connect(Show);
ping::Fire(6);
ping::Clear();
post(subscription.Connected);
post(One());
post(Point { x = 3, y = 4 }.x);
