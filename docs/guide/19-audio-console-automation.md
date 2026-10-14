# 19. Sound, console and automation

Beyond graphics, the standard library covers sound, rich console I/O, JSON, dates and desktop
automation. Each is a module imported with `link @clpp.<name>`.

## `@clpp.audio`: sound

All arguments are required: Tone takes frequency, seconds and wave; Sweep takes start/end frequency, seconds and wave; Play takes sound, volume, pan, pitch and loop. For example, use `Audio.Tone(440, 0.1, "sine")` and `Audio.Play(sound, 1, 0, 1, false)`.

The execution owns its decoded sounds, voices, mixer thread and audio device. After the program and its workers finish, the VM stops playback, joins the mixer thread, closes the device and frees sample/voice storage, including after an unhandled error. Sound and voice handles must not be reused in a later execution. Use Stop or StopAll to stop playback earlier; ending a lexical block does not release these handles.

Sounds are decoded once into 44.1 kHz stereo and mixed in software; on Windows the mix goes to the
default device through `waveOut` (no extra DLLs). Without a device — other systems for now,
`CLPP_HEADLESS`, or no sound card — everything still works except that nothing is heard, so
programs and tests behave the same. `Audio.SaveWav` writes any sound to a `.wav` file.

```clp
link @clpp.audio as Audio;
number coin = Audio.Sweep(880, 1760, 0.12, "square");   << a rising arcade "coin"
number note = Audio.Tone(440, 0.3, "sine");                     << a 440 Hz sine for 0.3 s
post(Audio.Duration(coin) > 0);
post(Audio.SaveWav(note, "note.wav"));
```

```output
true
true
```

| Function | Does |
| --- | --- |
| `Audio.Load(path)` | loads a `.wav` file, returns a sound handle |
| `Audio.Tone(freq, seconds, wave)` | a synthesized tone (`wave`: `"sine"`, `"square"`, `"saw"`, `"triangle"`, `"noise"`) |
| `Audio.Sweep(from, to, seconds, wave)` | a tone whose pitch slides from `from` to `to` |
| `Audio.Play(sound, volume, pan, pitch, loop)` | starts the sound, returns a voice handle |
| `Audio.Stop(voice)` / `Audio.StopAll()` | stops one voice / all |
| `Audio.SetVolume/SetPan/SetPitch(voice, …)` | changes a playing voice (`pan` −1 left … 1 right) |
| `Audio.IsPlaying(voice)` | whether a voice is still sounding |
| `Audio.MasterVolume(v)`, `Audio.Available()`, `Audio.Playing()` | master gain, device present, voices playing |
| `Audio.Duration(sound)`, `Audio.SaveWav(sound, path)` | length in seconds, export to WAV |

```clp target fragment
link @clpp.audio as Audio;
number music = Audio.Load("theme.wav");
number voice = Audio.Play(music, 0.6, 0, 1, true);
number shot = Audio.Tone(220, 0.08, "square");
<< later, on an event:
Audio.Play(shot, 1, -0.3, 1, false);
Audio.SetVolume(voice, 0.3);
```

## Audio validation

Time.Sleep and Input.Wait reject non-finite durations and positive delays beyond
2,147,483,647 milliseconds. Input coordinates are checked against native signed
32-bit ranges before desktop operations; relative movement also checks the
resulting position, and scrolling checks its scaled wheel delta.

Audio sound/voice handles are validated before native integer conversions.
Unknown or out-of-range sound handles fail in Play, Duration and SaveWav.
Stopping an unknown voice remains harmless, and IsPlaying returns false.
Numeric playback/control arguments and tone parameters must be finite. Tone
frequencies must stay within the 44,100 Hz sample rate in either direction;
synthesis duration remains bounded to 60 seconds.

## Typed JSON values

Json.ParseValue returns a recursive Json.Value. Its Type discriminator is Json.Kind::Null, Boolean, Integer, Decimal, Text, Array or Object. Read the matching Boolean, Integer, Decimal or Text field, Items array, or Entries dictionary. Inactive fields have neutral defaults; inspect Type before selecting a payload. Json.StringifyValue preserves booleans, signed 64-bit integers and decimal categories (including 1.0 and negative zero). It rejects cycles, excessive nesting and invalid payloads. Integer overflow and malformed Unicode surrogate escapes fail parsing with positions. The older Parse/Get APIs remain explicitly dynamic compatibility interfaces.

```clp
link @clpp.json as Json;
auto value = Json.ParseValue("{\"enabled\":true,\"score\":9007199254740993}");
post(value.Entries["enabled"].Boolean);
post(value.Entries["score"].Integer);
post(Json.StringifyValue(value));
```

```output
true
9007199254740993
{"enabled":true,"score":9007199254740993}
```

## `@clpp.io`: the console

`@clpp.io` prints without a trailing newline, reads input, and controls colors and the cursor
with ANSI escapes. When the output is **not** a terminal (a pipe, a file, the test suite), the
color and cursor calls write nothing, so logs stay clean.

```clp
link @clpp.io as Io;
Io.Print("Loading");
number dot = 0;
for (dot = 0; dot < 3; dot += 1) { Io.Print("."); }
Io.Print("\n");
post("done");
```

```output
Loading...
done
```

| Function | Does |
| --- | --- |
| `Io.Print(value)` / `Io.PrintError(value)` | writes without a newline, to stdout / stderr |
| `Io.ReadLine()` / `Io.Prompt(question)` | reads a line (optionally after printing a prompt) |
| `Io.ReadNumber(question)` | asks until the answer is a number |
| `Io.ReadKey()` | one key, without Enter (`"Up"`, `"Enter"`, `"a"`, …) |
| `Io.KeyAvailable()` | whether a key is waiting |
| `Io.Color(name)` / `Io.Background(name)` | sets the text / background color |
| `Io.ColorRgb(r, g, b)` / `Io.Reset()` | a true-color text color / back to default |
| `Io.Clear()`, `Io.MoveTo(col, row)`, `Io.ClearLine()` | clear the screen, move the cursor |
| `Io.Columns()` / `Io.Rows()` / `Io.IsTerminal()` | the terminal size and whether it is one |

Color names: `black red green yellow blue magenta cyan white`, their `bright*` variants, and
`bold` / `underline`. A colored progress bar in the terminal:

```clp target fragment
link @clpp.io as Io;
void bar(float fraction) {
  Io.ClearLine();
  Io.Color("green");
  float filled = fraction * 20;
  number i = 0;
  for (i = 0; i < 20; i += 1) { Io.Print(i < filled ? "█" : "·"); }
  Io.Reset();
  Io.Print(" " .: (fraction * 100) .: "%");
}
```

## `@clpp.input`: desktop automation

`@clpp.input` drives the real mouse and keyboard and reads the screen — the building blocks for
macros, UI automation and end-to-end tests of other apps, in the spirit of AutoHotkey and Python's
pyautogui. Everything acts **visibly** in your own session: the cursor moves where you can see it
and keys go to the focused window. Every function is refused inside `actor(...)`. Windows is the
full implementation; other systems report that a call is not supported yet.

```clp target fragment
link @clpp.input as Input;
post(Input.ScreenWidth() .: "x" .: Input.ScreenHeight());
post(Input.MouseX() .: ", " .: Input.MouseY());
number color = Input.Pixel(10, 10);     << the screen pixel at (10, 10), 0xRRGGBB
Input.ClickAt(color >= 0 ? 100 : 0, 100, 0);
```

| Function | Does |
| --- | --- |
| `Input.Move(x, y)` / `Input.MoveBy(dx, dy)` | moves the cursor |
| `Input.Click(button)` / `DoubleClick` / `MouseDown` / `MouseUp` | mouse buttons (0 left, 1 right, 2 middle) |
| `Input.ClickAt(x, y, button)` | moves then clicks |
| `Input.Scroll(amount)` | the mouse wheel (positive up) |
| `Input.Press(key)` / `KeyDown(key)` / `KeyUp(key)` | a key, e.g. `"enter"`, `"a"`, `"f5"` |
| `Input.Hotkey("ctrl+shift+s")` | a key combination |
| `Input.Type(text)` | types a string (any Unicode, not just keys) |
| `Input.IsDown(key)` | whether a key is physically held right now |
| `Input.MouseX()` / `Input.MouseY()` | the global cursor position |
| `Input.Pixel(x, y)` | the color of a screen pixel |
| `Input.Capture(x, y, w, h)` | a screen region as a `@clpp.gfx` image handle |
| `Input.ScreenWidth()` / `Input.ScreenHeight()` | the screen size |
| `Input.Wait(ms)` | pauses between macro steps |

A small macro — open Run, type a command, press Enter:

```clp fragment
link @clpp.input as Input;
Input.Hotkey("win+r");
Input.Wait(300);
Input.Type("notepad");
Input.Press("enter");
```

Reading a pixel to wait for something on screen, then clicking it:

```clp target fragment
link @clpp.input as Input;
while (Input.Pixel(500, 400) != 0x2ECC71) {   << wait for the button to turn green
  Input.Wait(50);
}
Input.ClickAt(500, 400, 0);
```

## `@clpp.json`: JSON

Objects become dictionaries and arrays become lists. `Get` and `Has` accept a dotted path. `Set` changes a direct key or index and returns a changed copy; it does not traverse a dotted path. JSON null and booleans retain distinct runtime values. Use ParseValue/StringifyValue above for the dedicated typed representation, including exact integer and decimal categories; Parse/Get remain explicitly dynamic APIs.

```clp
link @clpp.json as Json;
auto data = Json.Parse("{\"name\": \"Ada\", \"hp\": 90, \"items\": [\"sword\", \"map\"]}");
post(data["name"]);
post(Json.Get(data, "items.1"));
post(Json.Has(data, "pos"));
auto save = Json.Object();
auto updatedSave = Json.Set(save, "level", 7);
post(Json.Stringify(updatedSave));
```

```output
Ada
map
false
{"level":7}
```

| Function | Does |
| --- | --- |
| `Json.Parse(text)` / `Json.Valid(text)` | text → value / check it parses |
| `Json.Stringify(value)` / `Json.Pretty(value)` | value → compact / indented text |
| `Json.Get(value, path)` / `Json.Has(value, path)` | read by `"a.b.0"` path |
| `Json.Set(target, key, value)` | a changed copy of a dictionary or list |
| `Json.Object()` / `Json.Keys(object)` | a new empty object / its keys |

JSON pairs with `@clpp.fs` to load and save data files:

```clp fragment
link @clpp.json as Json;
link @clpp.fs as Fs;
auto config = Json.Parse(Fs.read("config.json"));
auto volume = Json.Get(config, "audio.volume");
Fs.write("save.json", Json.Pretty(Json.Set(Json.Object(), "score", 1200)));
```

## `@clpp.time`: clocks and dates

```clp
link @clpp.time as Time;
post(Time.Now() > 0);         << seconds since 1970
post(Time.Clock() >= 0);      << seconds since the program started (monotonic)
post(Time.Year() >= 2024);
```

```output
true
true
true
```

`Time.Sleep(ms)`, `Time.Date()` (`"2026-10-01"`), `Time.TimeOfDay()` (`"14:30:00"`),
`Time.Format(pattern)` (strftime), and the parts `Year Month Day Hour Minute Second Weekday
DayOfYear`.

## `@clpp.fs`: reading and writing files

Chapter 14 covered reading; `@clpp.fs` also writes. Paths containing `..` are refused, and code
inside `actor(...)` has no file access.

```clp
link @clpp.fs as Fs;
post(Fs.write("greeting.txt", "hello"));
post(Fs.read("greeting.txt"));
post(Fs.exists("greeting.txt"));
Fs.append("greeting.txt", " world");
post(Fs.read("greeting.txt"));
```

```output
true
hello
true
hello world
```

| Function | Does |
| --- | --- |
| `Fs.read(path)` / `Fs.size(path)` / `Fs.list(folder)` | read a file / its size / a folder's names |
| `Fs.write(path, text)` / `Fs.append(path, text)` | write / append (returns whether it worked) |
| `Fs.exists(path)` / `Fs.isDir(path)` | whether a path exists / is a folder |
| `Fs.remove(path)` / `Fs.makeDir(path)` | delete a file or empty folder / create folders |

Together these modules make CL++ a practical language for real programs: a game with graphics and
sound, a desktop tool with a GUI, a macro that automates a repetitive task, or a script that reads
and writes JSON configuration — all from one small executable with no dependencies.
