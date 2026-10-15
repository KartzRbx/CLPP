<< Editor smoke test: canonical definitions, contextual captures and events.
<< --------------------------------------------------------------------
<< 5) def com HERANÇA nominal.
<< --------------------------------------------------------------------
<<[ Brackets inside block comments: ] > ]] > ]>] text ]>>
def Transform<T> = (T) -> T;
template <typename T>
Transform<T> Keep(T initial) {
  return func(value) -> T { return initial; };
}
def Box<T> { T Value; }
T Box::Read() { return @this.Value; }
signal<number> changed;
auto subscription = changed~>Connect(func(value) -> void { post(value); });
changed::Fire(Keep<number>(7)(9));
changed::Clear();
array<bool> flags = { true, false };
post("ready");
<<[ multiline
   comment ]>>
extern number strlen(string text);
def Amount { number Value; }
Amount operator+(Amount left, Amount right) { return Amount { Value = left.Value + right.Value }; }

number attempts = 0; do { attempts += 1; } while(attempts < 3);

switch (attempts) {
  case 1: post("one"); break;
  case 3: post("three"); break;
  default: post("other");
}
float converted = static_cast<float>(attempts);

auto storedFactory = Keep<number>;
decltype(attempts) inferred = 7;
def Defaults<T> {
  array<T> Items = array<T>();
  Transform<T> Apply = func(value) -> T { return value; };
}
Vector3 position = Vector3(1, 2, 3);
buffer bytes = buffer::create(8);
number pontuação = 7;
post(pontuação);
def Outcome = "ongoing" | "team_a" | "team_b";
auto typedOutcome = (Outcome "ongoing");
if (typedOutcome != (Outcome "team_a")) { post("typed outcome"); }
post((attempts - 1));
