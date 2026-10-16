<< Interface declarativa com @clpp.gui: painel, controles e eventos a cada frame.
link @clpp.gui as Gui;
link @clpp.gfx as Gfx;
link @clpp.window as Window;
link @clpp.axiom as Axiom;

auto status = "Ajuste as opções e clique em Jogar.";

any menu(string message, float load) {
  return Gui.Screen(Gui.ScreenOptions {color = Gfx.DARK, children = list(
    Gui.Frame(Gui.FrameOptions {id = "menu", anchor = "center", width = 440, glass = true, appear = "pop", padding = 18, gap = 10, children = list(
      Gui.Text(Gui.TextOptions {text = "Aventura CL++", size = 28, bold = true, align = "center"}),
      Gui.Text(Gui.TextOptions {text = message, muted = true, align = "center"}),
      Gui.Divider(),
      Gui.TextBox(Gui.TextBoxOptions {id = "player", text = "Jogador", placeholder = "Seu nome…", value = "Ada"}),
      Gui.Slider(Gui.SliderOptions {id = "volume", text = "Volume", min = 0, max = 100, value = 70, step = 5}),
      Gui.Toggle(Gui.ToggleOptions {id = "music", text = "Música", value = true}),
      Gui.Checkbox(Gui.CheckboxOptions {id = "fx", text = "Efeitos sonoros", value = true}),
      Gui.Choice(Gui.ChoiceOptions {id = "quality", text = "Gráficos", options = "Baixa|Média|Alta", value = 1}),
      Gui.Progress(Gui.ProgressOptions {text = "Carregando recursos", value = load}),
      Gui.Row(Gui.RowOptions {gap = 12, children = list(
        Gui.Button(Gui.ButtonOptions {id = "play", text = "Jogar", style = "primary", grow = true}),
        Gui.Button(Gui.ButtonOptions {id = "quit", text = "Sair", style = "ghost", grow = true})
      )})
    )})
  )});
}

string refresh_status() {
  auto name = Gui.GetText("player");
  if (name == "") {
    name = "Jogador";
  }
  auto quality = "Baixa";
  if (Gui.Selected("quality") == 1) {
    quality = "Média";
  } else if (Gui.Selected("quality") == 2) {
    quality = "Alta";
  }
  return `${name} · vol ${Gui.Value("volume")} · ${quality} · música ` .:
    (Gui.IsOn("music") ? "sim" : "não") .: " · FX " .: (Gui.IsOn("fx") ? "sim" : "não");
}

Window.Open("CL++ — Interface (@clpp.gui)", 720, 520);
Window.SetFps(120);

while (Window.Frame()) {
  const auto load = Axiom.PingPong(Window.Time() * 40, 100);
  Gfx.Gradient(0, 0, Window.Width(), Window.Height(), Gfx.DARK, 0x14141F);
  Gui.Render(menu(status, load));

  if (Gui.Changed("volume") || Gui.Changed("music") || Gui.Changed("fx") || Gui.Changed("quality") ||
      Gui.Changed("player")) {
    status = refresh_status();
  }
  if (Gui.Clicked("play")) {
    status = "Iniciando partida… " .: refresh_status();
    Window.SetTitle("CL++ — " .: Gui.GetText("player"));
  }
  if (Gui.Clicked("quit")) {
    break;
  }
  if (Window.IsHeadless() && Window.FrameCount() >= 3) {
    break;
  }
}

Window.Close();
post("Interface encerrada.");
