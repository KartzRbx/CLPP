link "./lib/collections.clp" as Collections;
def Transform<T> = (T) -> T;
template <typename T>
Transform<T> Remember(T initial) {
  T previous = initial;
  return func(value) -> T {
    T result = previous;
    previous = value;
    return result;
  };
}
auto prior = Remember<string>("start");
post(prior("alpha"));
post(prior("beta"));
array<number> input = { 1, 2, 3, 4, 5, 6 };
auto even = Collections.Filter<number>(input, func(value) -> bool { return value % 2 == 0; });
auto labels = Collections.Map<number,string>(even, func(value) -> string { return "item:" .: value; });
for (string label in labels) { post(label); }
post(len(input)); << Transformations preserve the original collection.
