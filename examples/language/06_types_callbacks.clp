enum class State { Ready, Done }
def Outcome = "ongoing" | "team_a" | "team_b";
def NumberLike = number | float;
def Transform<T> = (T) -> T;
def Getter<T> = () -> T;
def Box<T> { T Value; template <typename U> U Echo(U value) { return value; } }
template <typename T> T Identity(T value) { return value; }
template <typename T> Getter<T> Keep(T value) { return func() -> T { return value; }; }
template <typename T : number | float> T Twice(T value) where T: number | float { return value + value; }
Transform<number> increment = func(value) -> number { return value + 1; };
Transform<number> identity = Identity<number>;
auto retained = Keep("captured");
Box<number> box = Box<number> { Value = 7 };
Outcome result = "ongoing";
optional<Box<number>> found = box;
guard (found != null) else { report("missing"); }
post(found.Value);
post(increment(identity(4)));
post(retained());
post(box.Echo<string>("typed"));
post(Twice(3));
match (State::Ready) { State::Ready => post(result); State::Done => post("done"); }
post((Outcome "team_b"));
