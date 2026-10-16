<< Demo: HUD + inventário. Evite `glass: true` em produção — blur na CPU a cada frame derruba o FPS.
link @clpp.gui as Gui;
link @clpp.gfx as Gfx;
link @clpp.ui as Ui;
link @clpp.window as Window;
link @clpp.axiom as Axiom;
link "./ui/hud.clp" as Hud;
link "./ui/inventory.clp" as Inventory;


auto inventoryOpen = false;
auto selectedSlot = -1;
auto toast = "Abra o inventário pelo HUD.";

any screen(float hp, float mp, bool invOpen, number slot) {
  if (invOpen) {
    return Gui.Screen(Gui.ScreenOptions {color = Gfx.TRANSPARENT, children = list(
      Hud.layer(hp, mp),
      Inventory.layer(true, slot)
    )});
  }
  return Gui.Screen(Gui.ScreenOptions {color = Gfx.TRANSPARENT, children = list(
    Hud.layer(hp, mp)
  )});
}

void backdrop() {
  const auto w = Window.Width();
  const auto h = Window.Height();
  Gfx.Gradient(0, 0, w, h, 0x0D0818, 0x1A1035);
}

number poll_slot_click() {
  for (auto i in 10) {
    const auto id = "inv_" .: i .: "_btn";
    if (Gui.Clicked(id)) {
      return i;
    }
  }
  return -1;
}

Ui.Theme("dark");
Ui.Accent(0xF472B6);

Window.Open("CL++ · HUD & Inventário (anime)", 960, 640);
Window.SetFps(120);

while (Window.Frame()) {
  const auto hp = 55 + Axiom.PingPong(Window.Time() * 12, 20);
  const auto mp = 40 + Axiom.PingPong(Window.Time() * 9, 25);
  backdrop();
  Gui.Render(screen(hp, mp, inventoryOpen, selectedSlot));

  if (Gui.Clicked("hud_inventory")) {
    inventoryOpen = !inventoryOpen;
    toast = inventoryOpen ? "Inventário aberto." : "Inventário fechado.";
  }
  if (Gui.Clicked("hud_quest")) {
    toast = "Missões — em breve!";
  }
  if (Gui.Clicked("hud_menu")) {
    toast = "Menu principal — em breve!";
  }
  if (inventoryOpen) {
    const auto picked = poll_slot_click();
    if (picked >= 0) {
      selectedSlot = picked;
      toast = "Slot " .: picked .: " selecionado.";
    }
    if (Gui.Clicked("inv_close")) {
      inventoryOpen = false;
      toast = "Inventário fechado.";
    }
    if (Gui.Clicked("inv_use") && selectedSlot >= 0) {
      toast = "Usou item do slot " .: selectedSlot .: "!";
    }
  }

  if (Window.IsHeadless() && Window.FrameCount() >= 3) {
    break;
  }
}

Window.Close();
post(toast);
