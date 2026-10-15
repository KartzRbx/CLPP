abstract def Named { abstract string Name(); }
def Storage {
  private number Value = 10;
  public number Read() { return @this.Value; }
  number Add(number amount) { @this.Value += amount; return @this.Value; }
}
final def Player : Storage, Named {
  string Label = "Ada";
  override string Name() { return @this.Label; }
  override number Read() { return @this.Add(2); }
}
auto player = Player {};
Named named = player;
Storage base = player;
post(named.Name());
post(base.Read());
auto read = player.Read;
post(read());
def Point { number X; number Y; }
number Point::Sum() { return @this.X + @this.Y; }
post((Point { X = 3, Y = 4 }).Sum());
