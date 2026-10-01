// @clpp.ui — immediate-mode widgets (in the spirit of Dear ImGui) drawn with @clpp.gfx.
//
// There are no widget objects and no callbacks: each frame the program calls the widgets it wants
// to show, and each call draws the widget and reports what the user did:
//
//   Ui.Panel("Settings", 20, 20, 300, 260);
//   volume = Ui.Slider("Volume", volume, 0, 100);
//   if (Ui.Button("Play")) { ... }
//   Ui.EndPanel();
//
// Widgets inside a panel are laid out top to bottom (Ui.Row(n) puts the next n side by side).
// A widget is identified by its panel and label; "Label##id" shows "Label" but uses the whole
// string as identity, for two widgets with the same text. Input comes from @clpp.window, so
// Window.Simulate* drives the UI too (tests, automation).
//
// Look: smooth text, soft shadows, rounded corners, animated hover/press, focus rings. Every
// metric derives from the font size (Ui.FontSize), so the whole UI scales with one number.
// @clpp.gui (std_gui.cpp) builds declarative interfaces on top of the same drawing helpers.

#include "stdlib/gfx_text.hpp"
#include "stdlib/host.hpp"
#include "stdlib/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clpp::stdlib::host {

namespace {

struct Layout {
  bool active{false};
  std::string panel;
  double x{0};
  double y{0};
  double width{0};
  double cursor{0};  // y of the next widget
  int columns{1};    // Row(n): widgets left in the current row
  int column{0};
  double row_height{0};
};

struct State {
  long long frame{-1};
  std::string active;   // widget being pressed / dragged
  std::string focused;  // text field receiving keys
  bool last_hovered{false};
  bool focus_claimed{false};
  Layout layout;
  std::unordered_map<std::string, std::pair<double, double>> panel_offsets;
  std::unordered_map<std::string, std::pair<double, double>> drag_start;
  std::unordered_map<std::string, int> label_uses;  // per frame: "Label" used n times
};

State g_ui;

[[nodiscard]] Style& style() { return ui_style(); }
[[nodiscard]] double em(const double factor) { return std::round(style().font * factor); }
[[nodiscard]] double padding() { return em(0.9); }
[[nodiscard]] double widget_height() { return em(2.3); }
[[nodiscard]] double spacing() { return em(0.55); }
[[nodiscard]] double line() { return text_height(style().font); }

// "Play##menu" -> shown "Play", id "Play##menu".
[[nodiscard]] std::string_view shown(const std::string& label) {
  const std::size_t hash = label.find("##");
  return hash == std::string::npos ? std::string_view(label) : std::string_view(label).substr(0, hash);
}

void begin_frame_if_needed() {
  const Input& in = input();
  if (in.frame == g_ui.frame) {
    return;
  }
  g_ui.frame = in.frame;
  g_ui.label_uses.clear();
  if (!in.was_down[0]) {
    g_ui.active.clear();  // the button was up last frame: whatever was pressed before is released
  }
  if (in.down[0] && !in.was_down[0]) {
    g_ui.focus_claimed = false;  // a click this frame: a text field may claim focus
  }
}

// Widgets draw on the @clpp.gfx canvas; without one there is nothing to draw on.
[[nodiscard]] bool ready(std::string& error) {
  if (canvas().width <= 0 || canvas().height <= 0) {
    return fail(error, "Ui: no canvas: call Window.Open(title, width, height) or Gfx.Canvas(width, height) first");
  }
  begin_frame_if_needed();
  return true;
}

[[nodiscard]] std::string widget_id(const std::string& label) {
  std::string id = g_ui.layout.panel + "/" + label;
  const int use = g_ui.label_uses[id]++;
  if (use > 0) {
    id += "#" + std::to_string(use);
  }
  return id;
}

[[nodiscard]] bool inside(const Box& box) { return mouse_in(box); }

// Next slot of the automatic layout (or a default spot when no panel is open).
[[nodiscard]] Box next_box(const double height) {
  Layout& layout = g_ui.layout;
  if (!layout.active) {
    Box box{padding(), padding() + layout.cursor, canvas().width - 2 * padding(), height};
    layout.cursor += height + spacing();
    return box;
  }
  const double inner = layout.width - 2 * padding();
  const int columns = std::max(1, layout.columns);
  const double cell = (inner - spacing() * (columns - 1)) / columns;
  Box box{layout.x + padding() + layout.column * (cell + spacing()), layout.cursor, cell, height};
  layout.row_height = std::max(layout.row_height, height);
  if (++layout.column >= columns) {
    layout.cursor += layout.row_height + spacing();
    layout.column = 0;
    layout.columns = 1;
    layout.row_height = 0;
  }
  return box;
}

// Press/release logic shared by buttons, checkboxes and choices: true on release over the box.
[[nodiscard]] bool clicked(const std::string& id, const Box& box) {
  const Input& in = input();
  const bool hovered = inside(box);
  g_ui.last_hovered = hovered;
  if (hovered && in.down[0] && !in.was_down[0] && g_ui.active.empty()) {
    g_ui.active = id;
  }
  const bool released = !in.down[0] && in.was_down[0];
  return released && hovered && g_ui.active == id;
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(ui_theme) {
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Ui.Theme: expected \"dark\" or \"light\"");
  }
  return set_theme(args[0].text, error);
}

CLPP_NATIVE(ui_accent) {
  (void)out;
  if (!numbers(args, arity, error, "Ui.Accent")) {
    return false;
  }
  set_accent(static_cast<std::uint32_t>(static_cast<std::int64_t>(args[0].number)) & 0xFFFFFFU);
  return true;
}

CLPP_NATIVE(ui_font_size) {
  (void)out;
  if (!numbers(args, arity, error, "Ui.FontSize")) {
    return false;
  }
  style().font = std::clamp(args[0].number, 8.0, 96.0);
  return true;
}

CLPP_NATIVE(ui_color) {  // (which): 0 background, 1 panel, 2 text, 3 muted, 4 accent, 5 border
  (void)arity;
  (void)error;
  const Theme& theme = style().theme;
  const std::uint32_t colors[6] = {theme.background, theme.panel, theme.text, theme.muted, theme.accent, theme.border};
  const int which = std::clamp(static_cast<int>(num(args[0])), 0, 5);
  out = Value::number_of(colors[which]);
  return true;
}

CLPP_NATIVE(ui_panel) {  // (title, x, y, w, h)
  (void)out;
  if (arity != 5 || !args[0].is_string() || !numbers(args + 1, 4, error, "Ui.Panel")) {
    return error.empty() ? fail(error, "Ui.Panel: expected (string title, x, y, width, height)") : false;
  }
  if (!ready(error)) {
    return false;
  }
  const std::string title = args[0].text;
  auto& offset = g_ui.panel_offsets[title];
  const double x = std::round(args[1].number + offset.first);
  const double y = std::round(args[2].number + offset.second);
  const double w = args[3].number;
  const double h = args[4].number;
  const double bar = em(2.6);
  // drag by the title bar
  const Input& in = input();
  const std::string id = "panel:" + title;
  const Box title_box{x, y, w, bar};
  if (!shown(title).empty() && inside(title_box) && in.down[0] && !in.was_down[0] && g_ui.active.empty()) {
    g_ui.active = id;
    g_ui.drag_start[id] = {in.mouse_x - offset.first, in.mouse_y - offset.second};
  }
  if (g_ui.active == id && in.down[0]) {
    const auto start = g_ui.drag_start[id];
    offset = {in.mouse_x - start.first, in.mouse_y - start.second};
  }
  draw_card(Box{x, y, w, h}, style().theme.panel, em(0.8), true);
  if (!shown(title).empty()) {
    draw_label(shown(title), x + padding(), y + (bar - line()) / 2.0, style().theme.text, 0, true);
    fill_rect(x + padding(), y + bar - 1, w - 2 * padding(), 1, style().theme.border);
  }
  const double top = shown(title).empty() ? y + padding() : y + bar + padding() * 0.8;
  g_ui.layout = Layout{true, title, x, y, w, top, 1, 0, 0};
  set_clip(x + 1, y + 1, w - 2, h - 2);  // content that does not fit is cut at the panel edge
  return true;
}

CLPP_NATIVE(ui_end_panel) {
  (void)args;
  (void)arity;
  (void)out;
  (void)error;
  g_ui.layout = Layout{};
  clear_clip();
  return true;
}

CLPP_NATIVE(ui_row) {
  (void)out;
  if (!numbers(args, arity, error, "Ui.Row") || !ready(error)) {
    return false;
  }
  Layout& layout = g_ui.layout;
  if (layout.column != 0) {  // finish a half-filled row first
    layout.cursor += layout.row_height + spacing();
    layout.column = 0;
    layout.row_height = 0;
  }
  layout.columns = std::clamp(static_cast<int>(args[0].number), 1, 16);
  return true;
}

CLPP_NATIVE(ui_label) {  // (text, 0 normal | 1 muted | 2 heading)
  (void)out;
  if (arity != 2 || !args[0].is_string()) {
    return fail(error, "Ui.Label: expected a string");
  }
  if (!ready(error)) {
    return false;
  }
  const int kind = static_cast<int>(num(args[1]));
  const double size = kind == 2 ? em(1.45) : kind == 1 ? em(0.9) : style().font;
  const double lines = 1.0 + static_cast<double>(std::count(args[0].text.begin(), args[0].text.end(), '\n'));
  const Box box = next_box(text_height(size) * lines);
  draw_label(args[0].text, box.x, box.y, kind == 1 ? style().theme.muted : style().theme.text, size, kind == 2);
  g_ui.last_hovered = inside(box);
  return true;
}

CLPP_NATIVE(ui_separator) {
  (void)args;
  (void)arity;
  (void)out;
  if (!ready(error)) {
    return false;
  }
  const Box box = next_box(em(0.5));
  fill_rect(box.x, std::round(box.y + box.h / 2), box.w, 1, style().theme.border);
  return true;
}

CLPP_NATIVE(ui_space) {
  (void)out;
  if (!numbers(args, arity, error, "Ui.Space") || !ready(error)) {
    return false;
  }
  g_ui.layout.cursor += args[0].number;
  return true;
}

CLPP_NATIVE(ui_button) {  // (label, style: 0 normal | 1 primary | 2 ghost)
  if (arity != 2 || !args[0].is_string()) {
    return fail(error, "Ui.Button: expected a string label");
  }
  if (!ready(error)) {
    return false;
  }
  const std::string id = widget_id(args[0].text);
  const Box box = next_box(widget_height());
  const bool result = clicked(id, box);
  draw_button(shown(args[0].text), box, static_cast<int>(num(args[1])), id, g_ui.active == id && input().down[0]);
  out = boolean(result);
  return true;
}

CLPP_NATIVE(ui_button_at) {  // (label, x, y, w, h)
  if (arity != 5 || !args[0].is_string() || !numbers(args + 1, 4, error, "Ui.ButtonAt")) {
    return error.empty() ? fail(error, "Ui.ButtonAt: expected (string label, x, y, width, height)") : false;
  }
  if (!ready(error)) {
    return false;
  }
  const std::string id = widget_id("at:" + args[0].text);
  const Box box{args[1].number, args[2].number, args[3].number, args[4].number};
  const bool result = clicked(id, box);
  draw_button(shown(args[0].text), box, 0, id, g_ui.active == id && input().down[0]);
  out = boolean(result);
  return true;
}

CLPP_NATIVE(ui_checkbox) {  // (label, value, style: 0 checkbox | 1 switch) -> value
  if (arity != 3 || !args[0].is_string()) {
    return fail(error, "Ui.Checkbox: expected (string label, bool value)");
  }
  if (!ready(error)) {
    return false;
  }
  const std::string id = widget_id(args[0].text);
  const Box box = next_box(widget_height());
  bool value = num(args[1]) != 0;
  if (clicked(id, box)) {
    value = !value;
  }
  draw_toggle(shown(args[0].text), box, value, num(args[2]) != 0, id);
  out = boolean(value);
  return true;
}

CLPP_NATIVE(ui_slider) {  // (label, value, min, max) -> value
  if (arity != 4 || !args[0].is_string() || !numbers(args + 1, 3, error, "Ui.Slider")) {
    return error.empty() ? fail(error, "Ui.Slider: expected (string label, value, min, max)") : false;
  }
  if (!ready(error)) {
    return false;
  }
  const std::string id = widget_id(args[0].text);
  const double low = args[2].number;
  const double high = args[3].number;
  double value = std::clamp(args[1].number, std::min(low, high), std::max(low, high));
  const Box box = next_box(line() + widget_height());
  const Box hit{box.x, box.y + line(), box.w, widget_height()};
  const Input& in = input();
  if (inside(hit) && in.down[0] && !in.was_down[0] && g_ui.active.empty()) {
    g_ui.active = id;
  }
  const Box track = slider_track(hit);
  if (g_ui.active == id && in.down[0] && track.w > 0 && high != low) {
    value = low + (high - low) * std::clamp((in.mouse_x - track.x) / track.w, 0.0, 1.0);
  }
  g_ui.last_hovered = inside(hit);
  draw_slider(shown(args[0].text), box, value, low, high, id, g_ui.active == id);
  out = Value::number_of(value);
  return true;
}

CLPP_NATIVE(ui_progress) {  // (label, fraction)
  (void)out;
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Ui.Progress: expected (string label, float fraction)");
  }
  if (!ready(error)) {
    return false;
  }
  const bool labeled = !shown(args[0].text).empty();
  const Box box = next_box((labeled ? line() : 0) + em(0.6));
  draw_progress(shown(args[0].text), box, args[1].number, style().theme.accent);
  return true;
}

void pop_utf8(std::string& text) {
  while (!text.empty()) {
    const auto byte = static_cast<unsigned char>(text.back());
    text.pop_back();
    if ((byte & 0xC0U) != 0x80U) {
      break;
    }
  }
}

CLPP_NATIVE(ui_input) {  // (label, value, placeholder) -> value
  if (arity != 3 || !args[0].is_string() || !args[1].is_string()) {
    return fail(error, "Ui.Input: expected (string label, string value)");
  }
  if (!ready(error)) {
    return false;
  }
  const std::string id = widget_id(args[0].text);
  const bool labeled = !shown(args[0].text).empty();
  const double label_space = labeled ? line() + em(0.3) : 0;
  const Box box = next_box(label_space + widget_height());
  const Box field{box.x, box.y + label_space, box.w, widget_height()};
  const Input& in = input();
  std::string value = args[1].text;
  if (in.down[0] && !in.was_down[0]) {
    if (inside(field)) {
      g_ui.focused = id;
      g_ui.focus_claimed = true;
    } else if (g_ui.focused == id && !g_ui.focus_claimed) {
      g_ui.focused.clear();
    }
  }
  const bool focused = g_ui.focused == id;
  if (focused) {
    value += in.typed;
    if (key_pressed("backspace")) {
      pop_utf8(value);
    }
    if (key_pressed("enter") || key_pressed("escape") || key_pressed("tab")) {
      g_ui.focused.clear();
    }
  }
  g_ui.last_hovered = inside(field);
  if (labeled) {
    draw_label(shown(args[0].text), box.x, box.y, style().theme.text, 0, false);
  }
  draw_text_field(field, value, args[2].is_string() ? args[2].text : std::string{}, focused, id);
  out = Value::string_of(std::move(value));
  return true;
}

CLPP_NATIVE(ui_choice) {  // (label, "A|B|C", selected) -> selected
  if (arity != 3 || !args[0].is_string() || !args[1].is_string() || !args[2].is_number()) {
    return fail(error, "Ui.Choice: expected (string label, string options \"A|B|C\", int selected)");
  }
  if (!ready(error)) {
    return false;
  }
  const std::vector<std::string> options = split_options(args[1].text);
  int selected = std::clamp(static_cast<int>(args[2].number), 0, static_cast<int>(options.size()) - 1);
  const bool labeled = !shown(args[0].text).empty();
  const double label_space = labeled ? line() + em(0.3) : 0;
  const Box box = next_box(widget_height() + label_space);
  if (labeled) {
    draw_label(shown(args[0].text), box.x, box.y, style().theme.text, 0, false);
  }
  const Box bar{box.x, box.y + label_space, box.w, widget_height()};
  const std::string group = widget_id(args[0].text + ":choice");
  const double cell = bar.w / static_cast<double>(options.size());
  bool any_hover = false;
  for (std::size_t index = 0; index < options.size(); ++index) {
    const Box part{bar.x + cell * static_cast<double>(index), bar.y, cell, bar.h};
    if (clicked(group + ":" + options[index], part)) {
      selected = static_cast<int>(index);
    }
    any_hover = any_hover || inside(part);
  }
  draw_segments(bar, options, selected, group);
  g_ui.last_hovered = any_hover;
  out = Value::number_of(selected);
  return true;
}

CLPP_NATIVE(ui_hovered) {
  (void)args;
  (void)arity;
  (void)error;
  out = boolean(g_ui.last_hovered);
  return true;
}

CLPP_NATIVE(ui_cursor) {  // (0 x | 1 y | 2 width) of the next widget slot
  (void)arity;
  (void)error;
  const Layout& layout = g_ui.layout;
  const int which = static_cast<int>(num(args[0]));
  out = Value::number_of(which == 0 ? layout.x + padding() : which == 1 ? layout.cursor : layout.width - 2 * padding());
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kUiSource = R"clp(<< @clpp.ui: immediate-mode widgets drawn with @clpp.gfx, driven by @clpp.window input.
<< Call the widgets every frame; each returns what the user did ("Label##id" for repeated labels).

func Theme(string name) { ui::Theme(name); }
func Accent(int color) { ui::Accent(color); }
func FontSize(float size) { ui::FontSize(size); }
func Background() -> int { return ui::Color(0); }
func PanelColor() -> int { return ui::Color(1); }
func TextColor() -> int { return ui::Color(2); }
func MutedColor() -> int { return ui::Color(3); }
func AccentColor() -> int { return ui::Color(4); }
func BorderColor() -> int { return ui::Color(5); }
func Panel(string title, float x, float y, float width, float height) { ui::Panel(title, x, y, width, height); }
func EndPanel() { ui::EndPanel(); }
func Row(int columns) { ui::Row(columns); }
func Label(string text) { ui::Label(text, 0); }
func Muted(string text) { ui::Label(text, 1); }
func Heading(string text) { ui::Label(text, 2); }
func Separator() { ui::Separator(); }
func Space(float pixels) { ui::Space(pixels); }
func Button(string label) -> bool { return ui::Button(label, 0); }
func PrimaryButton(string label) -> bool { return ui::Button(label, 1); }
func GhostButton(string label) -> bool { return ui::Button(label, 2); }
func ButtonAt(string label, float x, float y, float width, float height) -> bool { return ui::ButtonAt(label, x, y, width, height); }
func Checkbox(string label, bool value) -> bool { return ui::Checkbox(label, value, 0); }
func Toggle(string label, bool value) -> bool { return ui::Checkbox(label, value, 1); }
func Slider(string label, float value, float min, float max) -> float { return ui::Slider(label, value, min, max); }
func Progress(string label, float fraction) { ui::Progress(label, fraction); }
func Input(string label, string value) -> string { return ui::Input(label, value, ""); }
func InputHint(string label, string value, string placeholder) -> string { return ui::Input(label, value, placeholder); }
func Choice(string label, string options, int selected) -> int { return ui::Choice(label, options, selected); }
func Hovered() -> bool { return ui::Hovered(); }
func CursorX() -> float { return ui::Cursor(0); }
func CursorY() -> float { return ui::Cursor(1); }
func ContentWidth() -> float { return ui::Cursor(2); }
)clp";

}  // namespace

void add_ui(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"ui::Theme", 1, ui_theme, false},
                                {"ui::Accent", 1, ui_accent, false},
                                {"ui::FontSize", 1, ui_font_size, false},
                                {"ui::Color", 1, ui_color, false},
                                {"ui::Panel", 5, ui_panel, false},
                                {"ui::EndPanel", 0, ui_end_panel, false},
                                {"ui::Row", 1, ui_row, false},
                                {"ui::Label", 2, ui_label, false},
                                {"ui::Separator", 0, ui_separator, false},
                                {"ui::Space", 1, ui_space, false},
                                {"ui::Button", 2, ui_button, false},
                                {"ui::ButtonAt", 5, ui_button_at, false},
                                {"ui::Checkbox", 3, ui_checkbox, false},
                                {"ui::Slider", 4, ui_slider, false},
                                {"ui::Progress", 2, ui_progress, false},
                                {"ui::Input", 3, ui_input, false},
                                {"ui::Choice", 3, ui_choice, false},
                                {"ui::Hovered", 0, ui_hovered, false},
                                {"ui::Cursor", 1, ui_cursor, false},
                            });
}

std::string_view ui_source() { return kUiSource; }

}  // namespace clpp::stdlib::host
