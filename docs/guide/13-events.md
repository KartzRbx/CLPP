# 13. Events and observable values

`signal` declares an event. Connect a handler with `~>`; calling the signal invokes its handlers. A handler can live in another module, so game systems can react without importing one another.

```clp
signal damaged;
func showDamage(int amount) {
  post("Damage: " .: amount);
}
damaged ~> showDamage;
damaged(15);
```

`observable` declares a value with change notifications. Register a callback with `OnChange`; it runs after each assignment and receives the new value.

```clp
observable int coins = 10;
coins.OnChange(func (int value) {
  post("Coins: " .: value);
});
coins = 25;
```

Events suit one-off actions such as damage or button clicks. Observables suit state that several parts of a program need to display or track.

Use a direct function call when the caller knows exactly who should do the work. A signal is useful when several independent modules react to the same occurrence. An observable is useful for changing state such as health, score, or settings. Avoid turning every field into an observable; extra callbacks make update order harder to reason about. Keep handlers short or delegate longer work to a named function.
