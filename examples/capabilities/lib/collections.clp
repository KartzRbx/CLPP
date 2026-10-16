export def Mapper<T,U> = (T) -> U;
export def Predicate<T> = (T) -> bool;
export template <typename T, typename U>
array<U> Map(array<T> input, Mapper<T,U> transform) {
  array<U> output = array<U>();
  for (T value in input) { push(output, transform(value)); }
  return output;
}
export template <typename T>
array<T> Filter(array<T> input, Predicate<T> predicate) {
  array<T> output = array<T>();
  for (T value in input) { if (predicate(value)) { push(output, value); } }
  return output;
}
