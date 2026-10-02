# 14. Standard library

Import a standard module with `link @clpp.name as Alias;`. Module declarations are visible to the compiler and editor, so names and argument types are checked. Built-in functions that need no import include `post`, `warn`, `report`, `len`, `list`, `push`, `pop`, `insert`, `remove`, `find`, `sort`, `args`, `pcall`, vector constructors, typed collections, and `buffer::create`.

| Module | Main purpose |
| --- | --- |
| `@clpp.axiom` | Mathematics for games |
| `@clpp.text` | String operations |
| `@clpp.math` | Basic numeric functions |
| `@clpp.fs` | File access |
| `@clpp.os` | Environment values |
| `@clpp.http` | URL host parsing |
| `@clpp.window`, `@clpp.gfx` | Window, input, and 2D rendering |
| `@clpp.ui`, `@clpp.gui` | Immediate and declarative interfaces |
| `@clpp.audio`, `@clpp.io`, `@clpp.input` | Sound, console, and automation |
| `@clpp.json`, `@clpp.time` | JSON and date/time utilities |

## Axiom

`Axiom.Clamp`, `Saturate`, `Wrap`, `PingPong`, `Snap`, `Round`, `Floor`, `Ceil`, `Trunc`, `Fract`, `Min`, `Max`, `Pow`, `Sqrt`, `Log`, and `Factorial` cover scalar math. `Lerp`, `InvLerp`, `Map`, `Approach`, `Smoothstep`, and `Smootherstep` support interpolation. Easing families include Sine, Quad, Cubic, Quart, Quint, Expo, Circ, Back, Elastic, and Bounce.

Vector helpers include `Dot`, `Cross`, `Length`, `Normalize`, `Distance`, `Angle`, `Project`, `Reflect`, `LerpVector2`, `LerpVector3`, `Slerp`, `LookAt`, `ClosestPointOnSegment`, and geometry tests. Angle conversion, trigonometry, deterministic value noise, random sampling, HSV color conversion, and Bézier curves are also available.

```clp
link @clpp.axiom as Axiom;
post(Axiom.Clamp(150, 0, 100));
post(Axiom.Distance(Vector3(0, 0, 0), Vector3(3, 4, 0)));
post(Axiom.Lerp(0, 100, 0.25));
```

## Files, environment, and URLs

`Fs.read(path)` reads text, `Fs.size(path)` returns its byte count, and `Fs.list(path)` lists directory entries. Paths containing `..` are rejected. `Os.env(name)` reads an environment variable. `Http.host(url)` extracts a URL host. These host facilities are unavailable inside an isolated `actor(...)`.

The [window and graphics](17-window-and-graphics.md), [interfaces](18-user-interfaces.md), and [audio and automation](19-audio-console-automation.md) chapters cover the remaining modules in detail.

## Where these modules fit

Use Axiom for interpolation, movement, easing, geometry, and color calculations in game or UI code. Use `Fs` and `Json` for local save files and configuration. Use `Window` and `Gfx` when you need to own a frame loop and draw pixels; use `Ui` or `Gui` when you need controls and layout. Use `Input` only for visible desktop automation that the user expects. `Os` and `Http` expose narrow host utilities; they do not form a general networking client. Avoid importing a module solely for one trivial operation that the language already provides as a built-in.
