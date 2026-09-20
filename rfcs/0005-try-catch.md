# S16 try/catch

`try { } catch (auto err) { }` emits `pcall`. `return` / `break` / `continue` inside try are tagged tables so they propagate. Never swallow. If the shape cannot be emitted, the diagnostic help is `pcall(function() ... end)`.
