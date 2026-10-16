<< Painel de inventário — abre com appear pop/fade; slots clicáveis com hover/press do motor de UI.
link @clpp.gui as Gui;
link @clpp.gfx as Gfx;

const auto PANEL = 0x141414;  << Cor fixa do painel.
const auto SLOT = 0x1E1E1E;   << Cor fixa dos slots.
const auto GOLD = 0xFFD56A;

export any item_slot(string id, string icon, string hint) {
  return Gui.Column(Gui.ColumnOptions {id = id, gap = 2, align = "center", width = 56, children = list(
    Gui.Button(Gui.ButtonOptions {id = id .: "_btn", text = icon, style = "ghost", width = 52, height = 52, color = 0xF472B6}),
    Gui.Text(Gui.TextOptions {text = hint, size = 10, muted = true, align = "center"})
  )});
}


export any layer(bool open, number selected) {
  return Gui.Frame(Gui.FrameOptions {id = "inventory_panel", anchor = "center", width = 420, padding = 16, gap = 10, visible = open, appear = "pop", glass = false, shadow = true, border = true, title = "Inventário", color = 0xF018122E, children = list(
      Gui.Text(Gui.TextOptions {text = "Toque um item · arraste o scroll mentalmente", muted = true, align = "center", size = 12}),
      Gui.Row(Gui.RowOptions {gap = 10, align = "center", children = list(
        item_slot("inv_0", "⚔", "Espada"),
        item_slot("inv_1", "🛡", "Escudo"),
        item_slot("inv_2", "🧪", "Poção"),
        item_slot("inv_3", "💎", "Gema"),
        item_slot("inv_4", "📜", "Scroll")
      )}),
      Gui.Row(Gui.RowOptions {gap = 10, align = "center", children = list(
        item_slot("inv_5", "🍙", "Onigiri"),
        item_slot("inv_6", "🎀", "Charm"),
        item_slot("inv_7", "✨", "Ether"),
        item_slot("inv_8", "🔮", "Orb"),
        item_slot("inv_9", "🌸", "Pétala")
      )}),
      Gui.Divider(),
      Gui.Text(Gui.TextOptions {text = selected >= 0 ? "Selecionado: slot " .: selected : "Nenhum item selecionado", align = "center", color = GOLD, bold = true}),
      Gui.Row(Gui.RowOptions {gap = 12, children = list(
        Gui.Button(Gui.ButtonOptions {id = "inv_use", text = "Usar", style = "primary", color = 0xF472B6, grow = true}),
        Gui.Button(Gui.ButtonOptions {id = "inv_close", text = "Fechar", style = "ghost", grow = true})
      )})
    )});
}

export number slot_index(string id) {
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
