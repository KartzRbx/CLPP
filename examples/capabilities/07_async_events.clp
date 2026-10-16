async number Receive(signal<number,string> ready) {
  tuple<number,string> payload = ready::Wait();
  post(payload[1]);
  return payload[0] * 2;
}
async number Square(number value) { return value * value; }
number SumRange(number count) {
  number total = 0;
  for (number value in count) { total += value; }
  return total;
}
signal<number,string> ready;
auto managed = ready~>Connect(func(value, label) -> void { post("event:" .: value); });
auto result = spawn Receive(ready);
ready::Fire(21, "payload delivered");
post(await result);
ready::Clear();
post(managed.Connected);
auto squares = parallel(Square(3), Square(4));
post(squares[0] + squares[1]);
auto first = thread SumRange(100);
auto second = thread SumRange(200);
post(join(first) + join(second));
