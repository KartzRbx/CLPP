# 17. Windows and graphics: `@clpp.window` and `@clpp.gfx`

CL++ can open a real window and draw into it, with no external libraries: the window is native
(Win32 on Windows) and all drawing is done on a software canvas by the compiler itself. The same
program runs **headless** — with no window — when the `CLPP_HEADLESS` environment variable is set
or the system has no window backend, so tests and build servers never block. In headless mode the
canvas still works and `Window.Frame()` returns `false` after `CLPP_HEADLESS` frames (default 60).

## The frame loop

A program owns its loop. `Window.Open` creates the window and `Window.Frame()` shows the last
frame, waits for the frame rate and reads input; it returns `false` when the window is closed.

```clp
link @clpp.window as Window;
link @clpp.gfx as Gfx;

Window.Open("Hello", 480, 320);
while (Window.Frame()) {
  Gfx.Clear(Gfx.DARK);
  Gfx.Text("Hello, CL++!", 40, 40, 24, Gfx.WHITE);
  Gfx.Circle(240, 200, 60, Gfx.ORANGE);
}
```

Run it and a window appears; press its close button (or Alt+F4) to end the loop. The same file
run with `CLPP_HEADLESS=1 clpp hello.clp` draws one frame off screen and exits.

## Drawing

Every shape takes a color as an `int` in the form `0xRRGGBB`. The top byte is **transparency**
(`0` opaque, `255` invisible), so a plain hex literal is an opaque color and `Gfx.Fade` /
`Gfx.Rgba` make translucent ones.

| Function | Draws |
| --- | --- |
| `Gfx.Clear(color)` | fills the whole canvas |
| `Gfx.Rect(x, y, w, h, color)` | a filled rectangle |
| `Gfx.RoundRect(x, y, w, h, radius, color)` | rounded rectangle |
| `Gfx.RectLine(x, y, w, h, [thickness,] color)` | rectangle outline |
| `Gfx.Circle(x, y, radius, color)` | filled circle (anti-aliased) |
| `Gfx.CircleLine(x, y, radius, [thickness,] color)` | circle outline |
| `Gfx.Line(x1, y1, x2, y2, [thickness,] color)` | a line |
| `Gfx.Triangle(x1, y1, x2, y2, x3, y3, color)` | filled triangle |
| `Gfx.Gradient(x, y, w, h, top, bottom)` | vertical gradient (`GradientH` for horizontal) |
| `Gfx.Text(text, x, y, size, color)` | text (`size` in pixels, `y` is the top) |
| `Gfx.Shadow(x, y, w, h, radius, blur, color)` | a soft drop shadow |
| `Gfx.Pixel(x, y, color)` / `Gfx.GetPixel(x, y)` | one pixel |

Named colors are provided as constants: `BLACK WHITE GRAY DARK LIGHT RED ORANGE YELLOW GREEN TEAL
CYAN BLUE PURPLE PINK BROWN`.

```clp
link @clpp.gfx as Gfx;
Gfx.Canvas(320, 200);                  << an off-screen canvas, no window
Gfx.Gradient(0, 0, 320, 200, 0x1E1E2E, 0x2B2B45);
Gfx.RoundRect(40, 40, 120, 80, 12, Gfx.BLUE);
Gfx.Circle(230, 90, 45, Gfx.Fade(Gfx.ORANGE, 0.8));
post(Gfx.Save("demo.png"));
```

```saida
true
```

`Gfx.Canvas(width, height)` makes a canvas without a window — for generating images on a server,
in a tool, or in a test. `Gfx.Save(path)` writes it as `.png` or `.bmp`.

## Colors

```clp
link @clpp.gfx as Gfx;
post(Gfx.Rgb(255, 136, 0));
post(Gfx.Hex("#FF8800"));
post(Gfx.Red(0xFF8800) .: "," .: Gfx.Green(0xFF8800) .: "," .: Gfx.Blue(0xFF8800));
post(Gfx.Mix(0x000000, 0xFFFFFF, 0.5));
```

```saida
16746496
16746496
255,136,0
8421504
```

| Function | Result |
| --- | --- |
| `Gfx.Rgb(r, g, b)` / `Gfx.Rgba(r, g, b, a)` | a color from 0–255 channels |
| `Gfx.Hsv(hue, saturation, value)` | a color from HSV (hue in degrees) |
| `Gfx.Hex("#FF8800")` | a color from a CSS hex string |
| `Gfx.Mix(a, b, t)` | blends two colors |
| `Gfx.Fade(color, alpha)` | makes a color translucent (`0` invisible, `1` opaque) |
| `Gfx.Red/Green/Blue(color)` | one channel, 0–255 |

## Text and fonts

Text is smooth by default (the system font through the OS on Windows); `Gfx.Font("pixel")`
switches to a built-in bitmap font that looks the same on every platform, and `Gfx.Font("Segoe
UI")` (or any family) picks a system font. `Gfx.Bold(true)` turns on bold. `Gfx.TextWidth(text,
size)` and `Gfx.TextHeight(size)` measure text for layout.

```clp
link @clpp.gfx as Gfx;
Gfx.Canvas(200, 60);
Gfx.Font("pixel");
Gfx.Text("Retro 8-bit", 10, 20, 12, Gfx.GREEN);
post(Gfx.TextWidth("Retro 8-bit", 12) > 0);
```

```saida
true
```

## Images

`Gfx.LoadImage(path)` reads a PNG or BMP and returns a handle; `Gfx.Image(handle, x, y)` draws it,
and `Gfx.ImageScaled` draws it at a size. `Gfx.Capture(x, y, w, h)` snapshots part of the canvas
into a new image.

```clp trecho
link @clpp.gfx as Gfx;
let logo = Gfx.LoadImage("logo.png");
Gfx.Image(logo, 20, 20);
Gfx.ImageScaled(logo, 200, 20, 64, 64);
```

## Effects (CPU "shaders")

`Gfx.Effect(name, amount)` applies a post-processing effect to the whole canvas (or to the current
clip region set by `Gfx.Clip`). These are the classic image filters, computed on the CPU:

`grayscale sepia invert brightness contrast saturation threshold posterize vignette scanlines
noise pixelate blur sharpen bloom chromatic`.

```clp
link @clpp.gfx as Gfx;
Gfx.Canvas(160, 160);
Gfx.Gradient(0, 0, 160, 160, Gfx.PURPLE, Gfx.CYAN);
Gfx.Circle(80, 80, 50, Gfx.YELLOW);
Gfx.Effect("bloom", 0.8);
Gfx.Effect("vignette", 0.6);
post(Gfx.Save("effects.png"));
```

```saida
true
```

Use them for glow on a game HUD, a frosted look behind a menu (`blur`), a retro CRT feel
(`scanlines` + `chromatic`), or a screenshot filter in a tool.

## Input

Inside the loop, `@clpp.window` reports the keyboard and mouse. Everything is edge-aware:
`*Down` is true while held, `*Pressed` is true on the frame it goes down, `*Released` on the frame
it comes up.

| Function | Returns |
| --- | --- |
| `Window.MouseX()` / `Window.MouseY()` | cursor position in the window |
| `Window.MouseDown(b)` / `MousePressed(b)` / `MouseReleased(b)` | mouse button `b` (0 left, 1 right, 2 middle) |
| `Window.Wheel()` | wheel movement this frame |
| `Window.KeyDown(name)` / `KeyPressed(name)` / `KeyReleased(name)` | a key, e.g. `"Space"`, `"A"`, `"Left"`, `"Escape"` |
| `Window.TypedText()` | the text typed this frame (for text fields) |
| `Window.Delta()` | seconds since the last frame (for frame-rate-independent motion) |
| `Window.Time()` | seconds since `Window.Open` |

```clp trecho
link @clpp.window as Window;
link @clpp.gfx as Gfx;
Window.Open("Move the dot", 400, 300);
let mut x = 200.0;
while (Window.Frame()) {
  if (Window.KeyDown("Left")) { x -= 200 * Window.Delta(); }
  if (Window.KeyDown("Right")) { x += 200 * Window.Delta(); }
  Gfx.Clear(Gfx.DARK);
  Gfx.Circle(x, 150, 20, Gfx.ORANGE);
  if (Window.KeyPressed("Escape")) { Window.Close(); }
}
```

## Simulating input

`Window.SimulateMouse`, `Window.SimulateKey` and `Window.SimulateText` inject input. They work in
headless mode too, which is how a test drives a UI without a real mouse:

```clp
link @clpp.window as Window;
link @clpp.gfx as Gfx;
Window.Open("test", 200, 120);
Window.SimulateMouse(100, 60, true);
Window.Frame();
post(Window.MouseDown(Window.LEFT));
post(Window.MouseX());
```

```saida
true
100
```

## Other window calls

| Function | Does |
| --- | --- |
| `Window.SetTitle(text)` | changes the title bar |
| `Window.SetFps(n)` | caps the frame rate (default 60) |
| `Window.Width()` / `Window.Height()` | the canvas size (follows resizes) |
| `Window.Fps()` | the measured frame rate |
| `Window.IsHeadless()` | whether it is running without a window |
| `Window.Alert(title, message)` / `Window.Confirm(title, message)` | a native dialog box |
| `Window.Close()` | ends the loop |

The next chapters build on this canvas: [chapter 18](18-user-interfaces.md) adds ready-made widgets
(buttons, sliders, a declarative UI tree) and [chapter 19](19-audio-console-automation.md) adds sound,
the console and desktop automation.
