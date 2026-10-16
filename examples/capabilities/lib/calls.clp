<< Reusable procedural cleanup registry; callbacks run once, in reverse order.
export def Cleanup = () -> void;
array<Cleanup> pending = array<Cleanup>();
export void Defer(Cleanup callback) { push(pending, callback); }
export number Pending() { return len(pending); }
export number Clean() {
  number failures = 0;
  while (len(pending) > 0) {
    Cleanup callback = pop(pending);
    try { callback(); }
    catch (error) { failures += 1; }
  }
  return failures;
}
