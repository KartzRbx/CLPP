# 21. Project: an account screen

This example builds a complete **local UI flow**: two fields, a submit button, validation feedback, and a success state. It shows how to organize a declarative screen around `@clpp.gui`. The current `Gui.TextBox` displays its contents and the standard library does not provide a network authentication service, so this is a UI prototype. A real login should use a host-provided secure input and authentication function; never store real account secrets in the CL++ source.

## Screen structure

`Gui.Screen` fills the window. A centered `Gui.Frame` contains labels, text boxes, status text, and buttons. Each interactive element has a stable `id`, which lets `Gui.GetText` and `Gui.Clicked` identify it after `Gui.Render`.

```clp
link @clpp.window as Window;
link @clpp.gfx as Gfx;
link @clpp.gui as Gui;

Window.Open("Account demo", 640, 420);
let mut message = "Enter a name and demo code";
let mut signedIn = false;

while (Window.Frame()) {
  Gfx.Clear(0x111827);
  Gui.Render(Gui.Screen(children: list(
    Gui.Frame(id: "account", title: "Account demo", anchor: "center", width: 360, children: list(
      Gui.Text(text: signedIn ? "Welcome!" : "Sign in", size: 26, bold: true),
      Gui.Text(text: "This form validates locally.", muted: true),
      Gui.TextBox(id: "name", placeholder: "Name"),
      Gui.TextBox(id: "code", placeholder: "Demo code"),
      Gui.Button(id: "submit", text: "Continue", style: "primary"),
      Gui.Text(text: message)
    ))
  )));

  if (Gui.Clicked("submit")) {
    let name = Gui.GetText("name");
    let code = Gui.GetText("code");
    if (len(name) == 0 or len(code) == 0) {
      message = "Fill in both fields";
    } else if (code == "demo") {
      signedIn = true;
      message = "Hello, " .: name;
    } else {
      message = "Invalid demo code";
    }
  }
}
```

Save the file as `account.clp` and run `clpp account.clp`. Enter any name and `demo` as the local code to reach the success state. For a real account system, replace the local comparison with an `extern func` supplied by the host application, provide a masked or secure input control, and handle authentication errors without exposing credentials in logs.

## Why the code is organized this way

The program keeps only two pieces of application state (`message` and `signedIn`). GUI elements keep their own text by `id` across frames. Rendering happens every frame, while `Gui.Clicked` reports an action for one frame only. Use this declarative style when a screen has a stable tree and several widgets. For quick debug panels that directly edit variables, [immediate-mode UI](18-user-interfaces.md) is usually simpler.
