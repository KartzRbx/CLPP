<< HUD lateral esquerdo — estilo JRPG / anime (vidro, barras, botões com hover nativo do @clpp.gui).
link @clpp.gui as Gui;

const PANEL = 0x141025;
const PORTRAIT = 0x2D1B4E;
const HP = 0xFF5A8A;
const MP = 0x5EC8FF;

func layer(float hp, float mp) {
  return Gui.Frame(
    id: "hud_left",
    anchor: "top-left",
    x: 14,
    y: 14,
    width: 124,
    padding: 10,
    gap: 8,
    glass: false,
    shadow: true,
    border: true,
    color: 0xF0141025,
    children: list(
      Gui.Text(text: "PARTY", size: 11, muted: true, bold: true),
      Gui.Frame(id: "hud_portrait", width: 96, height: 96, padding: 4, color: PORTRAIT, radius: 12, children: list(
        Gui.Text(text: "Sakura", bold: true, align: "center", size: 13, color: 0xFFD4EC),
        Gui.Text(text: "Lv. 24", muted: true, align: "center", size: 11)
      )),
      Gui.Progress(text: "HP", value: hp, color: HP, width: 0),
      Gui.Progress(text: "MP", value: mp, color: MP, width: 0),
      Gui.Divider(),
      Gui.Button(id: "hud_inventory", text: "Inventário", style: "primary", color: 0xF472B6),
      Gui.Button(id: "hud_quest", text: "Missões", style: "ghost"),
      Gui.Button(id: "hud_menu", text: "Menu", style: "ghost")
    )
  );
}
