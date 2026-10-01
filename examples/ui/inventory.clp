<< Painel de inventário — abre com appear pop/fade; slots clicáveis com hover/press do motor de UI.
link @clpp.gui as Gui;
link @clpp.gfx as Gfx;

const PANEL = 0x141414;  << module constants must be literals (Gfx.Rgb(20,20,20))
const SLOT = 0x1E1E1E;   << Gfx.Rgb(30,30,30)
const GOLD = 0xFFD56A;

func item_slot(string id, string icon, string hint) {
  return Gui.Column(id: id, gap: 2, align: "center", width: 56, children: list(
    Gui.Button(id: id .: "_btn", text: icon, style: "ghost", width: 52, height: 52, color: 0xF472B6),
    Gui.Text(text: hint, size: 10, muted: true, align: "center")
  ));
}


func layer(bool open, int selected) {
  return Gui.Frame(
    id: "inventory_panel",
    anchor: "center",
    width: 420,
    padding: 16,
    gap: 10,
    visible: open,
    appear: "pop",
    glass: false,
    shadow: true,
    border: true,
    title: "Inventário",
    color: 0xF018122E,

    children: list(
      Gui.Text(text: "Toque um item · arraste o scroll mentalmente", muted: true, align: "center", size: 12),
      Gui.Row(gap: 10, align: "center", children: list(
        item_slot("inv_0", "⚔", "Espada"),
        item_slot("inv_1", "🛡", "Escudo"),
        item_slot("inv_2", "🧪", "Poção"),
        item_slot("inv_3", "💎", "Gema"),
        item_slot("inv_4", "📜", "Scroll")
      )),
      Gui.Row(gap: 10, align: "center", children: list(
        item_slot("inv_5", "🍙", "Onigiri"),
        item_slot("inv_6", "🎀", "Charm"),
        item_slot("inv_7", "✨", "Ether"),
        item_slot("inv_8", "🔮", "Orb"),
        item_slot("inv_9", "🌸", "Pétala")
      )),
      Gui.Divider(),
      Gui.Text(
        text: selected >= 0 ? "Selecionado: slot " .: selected : "Nenhum item selecionado",
        align: "center",
        color: GOLD,
        bold: true
      ),
      Gui.Row(gap: 12, children: list(
        Gui.Button(id: "inv_use", text: "Usar", style: "primary", color: 0xF472B6, grow: true),
        Gui.Button(id: "inv_close", text: "Fechar", style: "ghost", grow: true)
      ))
    )
  );
}

func slot_index(string id) -> int {
  if (id == "inv_0_btn") { return 0; }
  if (id == "inv_1_btn") { return 1; }
  if (id == "inv_2_btn") { return 2; }
  if (id == "inv_3_btn") { return 3; }
  if (id == "inv_4_btn") { return 4; }
  if (id == "inv_5_btn") { return 5; }
  if (id == "inv_6_btn") { return 6; }
  if (id == "inv_7_btn") { return 7; }
  if (id == "inv_8_btn") { return 8; }
  if (id == "inv_9_btn") { return 9; }
  return -1;
}
