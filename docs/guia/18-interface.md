# 18. User interfaces: `@clpp.ui` and `@clpp.gui`

CL++ ships two ways to build a graphical interface on top of the [canvas](17-janela-e-graficos.md),
both with the same modern look (smooth text, soft shadows, rounded corners, animated hover and
press):

- **`@clpp.ui`** — *immediate mode* (like Dear ImGui). Every frame you call the widgets you want;
  each call draws the widget and returns what the user did. Great for tools, debug panels and quick
  settings screens.
- **`@clpp.gui`** — *declarative* (like Roblox's Fusion/Roact). You describe the interface as a
  tree of elements with named properties and render it; the library lays it out and tells you what
  happened. Great for menus and app-like screens.

Both read input from `@clpp.window`, so `Window.Simulate*` drives them in tests.

## Immediate mode: `@clpp.ui`

```clp trecho
link @clpp.window as Window;
link @clpp.gfx as Gfx;
link @clpp.ui as Ui;

Window.Open("Settings", 360, 320);
let mut volume = 60.0;
let mut music = true;
let mut name = "Ada";

while (Window.Frame()) {
  Gfx.Clear(Ui.Background());
  Ui.Panel("Settings", 20, 20, 320, 280);
  Ui.Heading("Audio");
  if (Ui.PrimaryButton("Play")) { post("play!"); }
  volume = Ui.Slider("Volume", volume, 0, 100);
  music = Ui.Toggle("Music", music);
  name = Ui.Input("Name", name);
  Ui.EndPanel();
}
```

Each widget call both draws and reports:

| Widget | Call | Returns |
| --- | --- | --- |
| Button | `Ui.Button(label)`, `Ui.PrimaryButton`, `Ui.GhostButton` | `true` the frame it is clicked |
| Checkbox / switch | `Ui.Checkbox(label, value)`, `Ui.Toggle(label, value)` | the new value |
| Slider | `Ui.Slider(label, value, min, max)` | the new value |
| Text field | `Ui.Input(label, value)` | the new text |
| Segmented choice | `Ui.Choice(label, "A\|B\|C", selected)` | the selected index |
| Progress bar | `Ui.Progress(label, fraction)` | — |
| Text | `Ui.Label`, `Ui.Muted`, `Ui.Heading` | — |
| Layout | `Ui.Panel`/`Ui.EndPanel`, `Ui.Row(n)`, `Ui.Separator`, `Ui.Space(px)` | — |

Widgets inside a panel stack top to bottom; `Ui.Row(n)` puts the next `n` side by side. Two
widgets with the same label are told apart by writing `"Label##id"` — the part after `##` is the
identity and is not shown.

`Ui.Theme("dark")` / `Ui.Theme("light")` switch the palette, `Ui.Accent(color)` sets the accent,
and `Ui.FontSize(px)` scales the whole UI (every metric derives from it).

Because the widgets are driven by window input, a test can click them headless:

```clp
link @clpp.window as Window;
link @clpp.gfx as Gfx;
link @clpp.ui as Ui;

Window.Open("t", 200, 120);
let mut clicks = 0;
for (let frame in 4) {
  if (frame == 1) { Window.SimulateMouse(40, 40, true); }
  if (frame == 2) { Window.SimulateMouse(40, 40, false); }
  Window.Frame();
  Gfx.Clear(Ui.Background());
  if (Ui.ButtonAt("Go", 20, 28, 80, 28)) { clicks += 1; }
}
post(clicks);
```

```saida
1
```

## Declarative: `@clpp.gui`

With `@clpp.gui` you build the interface as a value — a tree of elements made by constructor
functions — and render it every frame. Interactive elements keep their state by `id` between
frames (the slider's value, the toggle, the text box), exactly like Roblox instances: the property
you pass is only the *starting* value.

```clp trecho
link @clpp.window as Window;
link @clpp.gfx as Gfx;
link @clpp.gui as Gui;

Window.Open("Menu", 800, 500);
while (Window.Frame()) {
  Gfx.Gradient(0, 0, 800, 500, 0x1A1530, 0x0D0D14);
  Gui.Render(Gui.Screen(children: list(
    Gui.Frame(id: "menu", title: "Main menu", anchor: "center", width: 340, appear: "pop", children: list(
      Gui.Text(text: "My Game", size: 30, bold: true, align: "center"),
      Gui.Text(text: "A tiny demo", muted: true, align: "center"),
      Gui.Button(id: "play", text: "Play", style: "primary"),
      Gui.Slider(id: "volume", text: "Volume", value: 70),
      Gui.Toggle(id: "music", text: "Music", value: true),
      Gui.Row(children: list(
        Gui.Button(id: "options", text: "Options", grow: true),
        Gui.Button(id: "quit", text: "Quit", style: "danger", grow: true)
      ))
    ))
  )));
  if (Gui.Clicked("play")) { post("play"); }
  if (Gui.Clicked("quit")) { Window.Close(); }
}
```

### Elements

| Constructor | Is |
| --- | --- |
| `Gui.Screen(children: ...)` | the root, filling the canvas |
| `Gui.Frame(...)` | a card with optional title, drag, shadow and `glass` blur |
| `Gui.Row(...)` / `Gui.Column(...)` | horizontal / vertical layout containers |
| `Gui.Text(...)` | a label (`size`, `bold`, `muted`, `color`, `align`) |
| `Gui.Button(...)` | a button (`style`: `"normal"`, `"primary"`, `"ghost"`, `"danger"`) |
| `Gui.Toggle` / `Gui.Checkbox(...)` | a switch / checkbox |
| `Gui.Slider(...)` | a slider (`min`, `max`, `value`, `step`) |
| `Gui.TextBox(...)` | a text field (`placeholder`) |
| `Gui.Choice(...)` | a segmented choice (`options: "A\|B\|C"`) |
| `Gui.Progress(...)` | a progress bar |
| `Gui.Image(...)` | a PNG/BMP, by `path` or image handle |
| `Gui.Spacer(size)`, `Gui.Divider()` | spacing helpers |

Common properties: `id` (its identity and how you read its state), `anchor` (`"center"`,
`"top-right"`, …), `x`/`y` (offset), `width`/`height`, `padding`, `gap`, `visible`, and `appear`
(`"fade"`, `"slide"`, `"pop"` for an entrance animation).

### Reading what happened

After `Gui.Render`, ask by `id`:

| Call | Tells you |
| --- | --- |
| `Gui.Clicked(id)` | a button was clicked this frame |
| `Gui.Changed(id)` | a slider / toggle / text box / choice changed |
| `Gui.Value(id)` | a slider's number |
| `Gui.IsOn(id)` | a toggle / checkbox |
| `Gui.GetText(id)` | a text box's text |
| `Gui.Selected(id)` | a choice's index |
| `Gui.Events()` | the list of ids that fired this frame |
| `Gui.SetValue/SetOn/SetText/SetSelected(id, …)` | change an element's state from code |

`Gui.Events()` pairs naturally with `match` and with `signal` (chapter 13) to wire a menu:

```clp
link @clpp.window as Window;
link @clpp.gfx as Gfx;
link @clpp.gui as Gui;

signal startGame;
func onStart() { post("starting"); }
startGame ~> onStart;

Window.Open("menu", 300, 200);
for (let frame in 4) {
  if (frame == 2) { Window.SimulateMouse(150, 100, true); }
  if (frame == 3) { Window.SimulateMouse(150, 100, false); }
  Window.Frame();
  Gfx.Clear(0x101018);
  Gui.Render(Gui.Screen(children: list(
    Gui.Button(id: "play", text: "Play", anchor: "center", width: 120)
  )));
  for (let e in Gui.Events()) {
    match (e) {
      "play" ~> startGame();
      _ ~> post("other: " .: e);
    }
  }
}
```

```saida
starting
```

### When to use which

- A debug panel, a tool, a quick options screen that mirrors your own variables → **`@clpp.ui`**:
  the widget returns the value, you keep it in a `let mut`.
- A menu or app screen with many elements, entrance animations and ids wired to signals →
  **`@clpp.gui`**: describe the tree, read `Gui.Clicked` / `Gui.Value`.

Both share one theme, so you can even mix them in the same window.
