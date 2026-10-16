signal<number, string> changed;
async number Read(signal<number, string> event) {
  tuple<number, string> payload = event::Wait();
  post(payload[1]);
  return payload[0];
}
auto once = changed::Once(func(value, label) -> void { post(value + 1); });
auto managed = changed~>Connect(func(value, label) -> void { post(label); });
auto pending = spawn Read(changed);
changed::Fire(41, "ready");
post(await pending);
changed::Clear();
post(managed.Connected);
post(once.Connected);
observable number score = 1;
auto observation = score.OnChange(func(value) -> void { post(value); });
score = 2;
observation::Disconnect();
def Async = (number) -> task<number>;
Async twice = async func(value) -> number { return value * 2; };
post(await spawn twice(3));
auto results = parallel(twice(2), twice(5));
post(results[0]); post(results[1]);
