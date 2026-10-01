// @clpp.gui — declarative interfaces, in the style of Roblox's Fusion/Roact.
//
// The program describes the interface as a tree of elements with named properties and renders it
// every frame; the library lays it out, draws it with the @clpp.ui look and reports events:
//
//   let screen = Gui.Screen(children: list(
//     Gui.Frame(id: "menu", anchor: "center", width: 320, children: list(
//       Gui.Text(text: "My game", size: 28, bold: true),
//       Gui.Button(id: "play", text: "Play", style: "primary"),
//       Gui.Slider(id: "volume", text: "Volume", value: 70)))));
//   Gui.Render(screen);
//   if (Gui.Clicked("play")) { playPressed(); }   // a CL++ signal: playPressed ~> startGame;
//
// Elements are plain lists of key/value pairs built by the module's constructor functions, so the
// tree is an ordinary CL++ value. Interactive elements keep their state by id between frames
// (slider value, toggle, text box text, choice), like Roblox instances; Gui.Value/SetValue and
// friends read and change it. Hit-testing finds the topmost element under the mouse first, so
// overlapping frames block what is under them.

#include "stdlib/gfx_text.hpp"
#include "stdlib/host.hpp"
#include "stdlib/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace clpp::stdlib::host {

namespace {

struct Node {
  std::string kind;
  std::vector<std::pair<std::string, Value>> props;
  std::vector<Node> children;
  std::string id;  // explicit or derived from the position in the tree
  Box box{0, 0, 0, 0};
  double width{0};   // measured
  double height{0};  // measured
};

struct ElementState {
  double value{0};
  bool on{false};
  std::string text;
  int selected{0};
  bool initialized{false};  // took its starting value from the element (`value:`)
  double appear{0};         // 0..1 appearance animation
  long long seen{-1};       // last Render that showed it
};

struct Gui {
  std::unordered_map<std::string, ElementState> states;
  std::string active;   // pressed element
  std::string focused;  // text box receiving keys
  std::string hovered;  // topmost interactive element under the mouse
  std::vector<std::string> events;  // ids clicked or changed in the last Render
  std::unordered_set<std::string> clicked;
  std::unordered_set<std::string> changed;
  long long frame{-1};
  long long renders{0};
};

Gui g_gui;

[[nodiscard]] double em(const double factor) { return std::round(ui_style().font * factor); }
[[nodiscard]] double control_height() { return em(2.3); }

// --- properties ---------------------------------------------------------------------------------

[[nodiscard]] const Value* prop(const Node& node, const std::string_view key) {
  for (const auto& [name, value] : node.props) {
    if (name == key) {
      return &value;
    }
  }
  return nullptr;
}

[[nodiscard]] std::string text_prop(const Node& node, const std::string_view key, const std::string& fallback = {}) {
  const Value* value = prop(node, key);
  return value != nullptr && value->is_string() ? value->text : fallback;
}

[[nodiscard]] double number_prop(const Node& node, const std::string_view key, const double fallback) {
  const Value* value = prop(node, key);
  return value != nullptr && value->is_number() ? value->number : fallback;
}

[[nodiscard]] bool flag_prop(const Node& node, const std::string_view key, const bool fallback) {
  const Value* value = prop(node, key);
  return value != nullptr && value->is_number() ? value->number != 0 : fallback;
}

// Colors: -1 means "the theme decides", -2 means "no fill".
[[nodiscard]] std::uint32_t color_prop(const Node& node, const std::string_view key, const std::uint32_t fallback) {
  const double raw = number_prop(node, key, -1);
  return raw < 0 ? fallback : static_cast<std::uint32_t>(static_cast<std::int64_t>(raw) & 0xFFFFFFFFLL);
}

[[nodiscard]] bool container(const Node& node) {
  return node.kind == "screen" || node.kind == "frame" || node.kind == "row" || node.kind == "column";
}

[[nodiscard]] bool interactive(const Node& node) {
  return node.kind == "button" || node.kind == "toggle" || node.kind == "checkbox" || node.kind == "slider" ||
         node.kind == "textbox" || node.kind == "choice";
}

[[nodiscard]] std::string layout_of(const Node& node) {
  if (node.kind == "screen") {
    return "none";
  }
  if (node.kind == "row") {
    return "horizontal";
  }
  return text_prop(node, "layout", "vertical");
}

// --- building the tree from CL++ values ---------------------------------------------------------

bool build(const Value& value, Node& node, const std::string& path, const int depth, std::string& error) {
  if (depth > 64) {
    error = "Gui.Render: the element tree is too deep";
    return false;
  }
  if (!value.is_struct() || value.number != 0 || value.fields.size() % 2 != 0) {
    error = "Gui.Render: expected an element made by Gui.Frame, Gui.Button, ... (got another value)";
    return false;
  }
  for (std::size_t index = 0; index + 1 < value.fields.size(); index += 2) {
    const Value& key = value.fields[index];
    if (!key.is_string()) {
      error = "Gui.Render: element keys must be strings";
      return false;
    }
    if (key.text == "kind" && value.fields[index + 1].is_string()) {
      node.kind = value.fields[index + 1].text;
    } else if (key.text == "children") {
      const Value& children = value.fields[index + 1];
      if (children.is_struct()) {
        for (std::size_t child = 0; child < children.fields.size(); ++child) {
          Node built;
          if (!build(children.fields[child], built, path + "/" + std::to_string(child), depth + 1, error)) {
            return false;
          }
          node.children.push_back(std::move(built));
        }
      }
    } else {
      node.props.emplace_back(key.text, value.fields[index + 1]);
    }
  }
  if (node.kind.empty()) {
    error = "Gui.Render: element without a kind";
    return false;
  }
  const std::string explicit_id = text_prop(node, "id");
  node.id = explicit_id.empty() ? path + ":" + node.kind + ":" + text_prop(node, "text") : explicit_id;
  return true;
}

// --- measuring and layout -----------------------------------------------------------------------

[[nodiscard]] double padding_of(const Node& node) {
  const double fallback = node.kind == "frame" ? em(1.1) : node.kind == "screen" ? em(1.0) : 0;
  return std::max(0.0, number_prop(node, "padding", -1) < 0 ? fallback : number_prop(node, "padding", 0));
}

[[nodiscard]] double gap_of(const Node& node) {
  return number_prop(node, "gap", -1) < 0 ? em(0.6) : number_prop(node, "gap", 0);
}

[[nodiscard]] double title_height(const Node& node) { return text_prop(node, "title").empty() ? 0 : em(2.4); }

[[nodiscard]] double text_size(const Node& node) {
  const double size = number_prop(node, "size", 0);
  return size > 0 ? size : ui_style().font;
}

void measure(Node& node, double available);

// Content size of a leaf element.
void measure_leaf(Node& node) {
  const double font = ui_style().font;
  const double line = text_height(font);
  const std::string text = text_prop(node, "text");
  double w = 0;
  double h = 0;
  if (node.kind == "text") {
    text::FontChoice& choice = text::choice();
    const bool was_bold = choice.bold;
    choice.bold = flag_prop(node, "bold", false);
    const double size = text_size(node);
    const double lines = 1.0 + static_cast<double>(std::count(text.begin(), text.end(), '\n'));
    w = text_width(text, size);
    h = text_height(size) * lines;
    choice.bold = was_bold;
  } else if (node.kind == "button") {
    w = text_width(text, font) + em(2.4);
    h = control_height();
  } else if (node.kind == "toggle" || node.kind == "checkbox") {
    w = em(3.2) + text_width(text, font);
    h = control_height();
  } else if (node.kind == "slider") {
    w = em(12);
    h = line + control_height();
  } else if (node.kind == "textbox") {
    w = em(12);
    h = (text.empty() ? 0 : line + em(0.3)) + control_height();
  } else if (node.kind == "choice") {
    w = em(4) * static_cast<double>(split_options(text_prop(node, "options", "A|B")).size());
    h = (text.empty() ? 0 : line + em(0.3)) + control_height();
  } else if (node.kind == "progress") {
    w = em(12);
    h = (text.empty() ? 0 : line) + em(0.6);
  } else if (node.kind == "image") {
    int iw = 0;
    int ih = 0;
    const int handle = static_cast<int>(number_prop(node, "image", 0));
    std::string ignored;
    const int resolved = handle != 0 ? handle : text_prop(node, "path").empty() ? 0 : load_image_file(text_prop(node, "path"), ignored);
    if (resolved != 0 && image_dimensions(resolved, iw, ih)) {
      w = iw;
      h = ih;
    }
  } else if (node.kind == "spacer") {
    w = number_prop(node, "size", em(0.6));
    h = w;
  } else if (node.kind == "divider") {
    w = em(2);
    h = em(0.6);
  }
  const double fixed_w = number_prop(node, "width", 0);
  const double fixed_h = number_prop(node, "height", 0);
  node.width = fixed_w > 0 ? fixed_w : w;
  node.height = fixed_h > 0 ? fixed_h : h;
  // an image with only one side given keeps its aspect ratio
  if (node.kind == "image" && w > 0 && h > 0) {
    if (fixed_w > 0 && fixed_h <= 0) {
      node.height = fixed_w * h / w;
    } else if (fixed_h > 0 && fixed_w <= 0) {
      node.width = fixed_h * w / h;
    }
  }
}

// Width a child takes inside a vertical container: fixed, or the whole inner width.
[[nodiscard]] bool stretches(const Node& child) {
  if (number_prop(child, "width", 0) > 0) {
    return false;
  }
  return child.kind != "text" && child.kind != "image" && child.kind != "spacer";
}

void measure(Node& node, const double available) {
  if (!container(node)) {
    measure_leaf(node);
    return;
  }
  const double pad = padding_of(node);
  const double fixed_w = number_prop(node, "width", 0);
  const double fixed_h = number_prop(node, "height", 0);
  const double outer = fixed_w > 0 ? fixed_w : available;
  const double inner = std::max(0.0, outer - 2 * pad);
  const std::string layout = layout_of(node);
  double content_w = 0;
  double content_h = 0;
  std::size_t visible = 0;
  for (Node& child : node.children) {
    if (!flag_prop(child, "visible", true)) {
      continue;
    }
    measure(child, inner);
    ++visible;
    if (layout == "horizontal") {
      content_w += child.width;
      content_h = std::max(content_h, child.height);
    } else if (layout == "vertical") {
      content_w = std::max(content_w, child.width);
      content_h += child.height;
    } else {
      content_w = std::max(content_w, child.width + number_prop(child, "x", 0));
      content_h = std::max(content_h, child.height + number_prop(child, "y", 0));
    }
  }
  if (visible > 1 && layout == "horizontal") {
    content_w += gap_of(node) * static_cast<double>(visible - 1);
  } else if (visible > 1 && layout == "vertical") {
    content_h += gap_of(node) * static_cast<double>(visible - 1);
  }
  // a frame stretches across its parent unless it was given a width; a row stretches too
  node.width = fixed_w > 0 ? fixed_w : (node.kind == "frame" && number_prop(node, "anchor_given", 0) != 0)
                                           ? std::min(available, content_w + 2 * pad)
                                           : available;
  node.height = fixed_h > 0 ? fixed_h : content_h + 2 * pad + title_height(node);
}

[[nodiscard]] std::pair<double, double> anchor_point(const std::string& anchor, const Box& area, const double w,
                                                     const double h) {
  double fx = 0;
  double fy = 0;
  if (anchor.find("right") != std::string::npos) {
    fx = 1;
  } else if (anchor.find("left") == std::string::npos && (anchor == "center" || anchor == "top" || anchor == "bottom")) {
    fx = 0.5;
  }
  if (anchor.find("bottom") != std::string::npos) {
    fy = 1;
  } else if (anchor.find("top") == std::string::npos && (anchor == "center" || anchor == "left" || anchor == "right")) {
    fy = 0.5;
  }
  return {area.x + (area.w - w) * fx, area.y + (area.h - h) * fy};
}

void place(Node& node, const Box& box);

void place_children(Node& node) {
  const double pad = padding_of(node);
  const double title = title_height(node);
  const Box inner{node.box.x + pad, node.box.y + pad + title, std::max(0.0, node.box.w - 2 * pad),
                  std::max(0.0, node.box.h - 2 * pad - title)};
  const std::string layout = layout_of(node);
  const double gap = gap_of(node);
  if (layout == "none") {
    for (Node& child : node.children) {
      if (!flag_prop(child, "visible", true)) {
        continue;
      }
      if (child.kind == "frame" && number_prop(child, "width", 0) <= 0 && text_prop(child, "anchor").empty()) {
        child.width = inner.w;  // an unanchored frame without width fills the area
      }
      const auto [x, y] = anchor_point(text_prop(child, "anchor", "top-left"), inner, child.width, child.height);
      place(child, Box{x + number_prop(child, "x", 0), y + number_prop(child, "y", 0), child.width, child.height});
    }
    return;
  }
  if (layout == "horizontal") {
    double fixed = 0;
    int growing = 0;
    std::size_t visible = 0;
    for (const Node& child : node.children) {
      if (!flag_prop(child, "visible", true)) {
        continue;
      }
      ++visible;
      if (flag_prop(child, "grow", false) || (container(child) && number_prop(child, "width", 0) <= 0) ||
          ((child.kind == "slider" || child.kind == "textbox" || child.kind == "progress") && number_prop(child, "width", 0) <= 0)) {
        ++growing;
      } else {
        fixed += child.width;
      }
    }
    const double room = inner.w - fixed - gap * static_cast<double>(visible > 0 ? visible - 1 : 0);
    double x = inner.x;
    const std::string align = text_prop(node, "align", "start");
    if (growing == 0 && room > 0) {
      x += align == "center" ? room / 2 : align == "end" ? room : 0;
    }
    for (Node& child : node.children) {
      if (!flag_prop(child, "visible", true)) {
        continue;
      }
      const bool grows = flag_prop(child, "grow", false) || (container(child) && number_prop(child, "width", 0) <= 0) ||
                         ((child.kind == "slider" || child.kind == "textbox" || child.kind == "progress") &&
                          number_prop(child, "width", 0) <= 0);
      const double w = grows ? std::max(0.0, room / growing) : child.width;
      if (container(child)) {
        measure(child, w);
      }
      const double y = inner.y + (inner.h - child.height) / 2;  // centered vertically in the row
      place(child, Box{x, child.kind == "text" ? y : inner.y + (inner.h - child.height) / 2, w, child.height});
      x += w + gap;
    }
    return;
  }
  // vertical
  double y = inner.y;
  const std::string align = text_prop(node, "align", "stretch");
  for (Node& child : node.children) {
    if (!flag_prop(child, "visible", true)) {
      continue;
    }
    const double w = stretches(child) && align == "stretch" ? inner.w : std::min(child.width, inner.w);
    double x = inner.x;
    const std::string child_align = text_prop(child, "align", "");
    const std::string& used = child.kind == "text" ? child_align : align;
    if (used == "center") {
      x += (inner.w - w) / 2;
    } else if (used == "end" || used == "right") {
      x += inner.w - w;
    }
    if (container(child) && w != child.width) {
      measure(child, w);
    }
    place(child, Box{x, y, child.kind == "text" ? std::max(w, child.width) : w, child.height});
    y += child.height + gap;
  }
}

void place(Node& node, const Box& box) {
  node.box = box;
  ElementState& state = g_gui.states[node.id];
  // appearance animation: slides/fades in the first frames after the element shows up
  const std::string appear = text_prop(node, "appear");
  if (state.seen < g_gui.frame - 1) {  // not shown by the previous Render: (re)appears now
    state.appear = appear.empty() ? 1.0 : 0.0;
  }
  if (!appear.empty() && state.appear < 1.0) {
    const double delta = input().delta;
    state.appear = std::min(1.0, state.appear + (delta <= 0 ? 1.0 : delta / 0.28));
  }
  state.seen = g_gui.frame;
  if (appear.find("slide") != std::string::npos || appear == "pop") {
    const double eased = 1.0 - std::pow(1.0 - state.appear, 3.0);
    node.box.y += (1.0 - eased) * em(1.6);
  }
  if (container(node)) {
    place_children(node);
  }
}

// --- hit testing -------------------------------------------------------------------------------

void collect_hits(const Node& node, std::vector<const Node*>& order) {
  if (!flag_prop(node, "visible", true)) {
    return;
  }
  if (node.kind != "screen" && node.kind != "row" && node.kind != "column" && node.kind != "text" &&
      node.kind != "spacer") {
    order.push_back(&node);
  }
  for (const Node& child : node.children) {
    collect_hits(child, order);
  }
}

// --- drawing and interaction -------------------------------------------------------------------

void mark_changed(const std::string& id) {
  if (g_gui.changed.insert(id).second) {
    g_gui.events.push_back(id);
  }
}

[[nodiscard]] bool press_released(const Node& node) {
  const Input& in = input();
  const bool over = g_gui.hovered == node.id;
  if (over && in.down[0] && !in.was_down[0] && g_gui.active.empty()) {
    g_gui.active = node.id;
  }
  return over && !in.down[0] && in.was_down[0] && g_gui.active == node.id;
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

void draw(const Node& node);

void draw_children(const Node& node) {
  for (const Node& child : node.children) {
    if (flag_prop(child, "visible", true)) {
      draw(child);
    }
  }
}

void draw_container(const Node& node) {
  const Theme& theme = ui_style().theme;
  const Box& box = node.box;
  if (node.kind == "screen") {
    const double raw = number_prop(node, "color", -1);
    if (raw >= 0) {
      fill_rect(box.x, box.y, box.w, box.h, color_prop(node, "color", theme.background));
    }
    draw_children(node);
    return;
  }
  const bool card = node.kind == "frame";
  const double raw_color = number_prop(node, "color", card ? -1 : -2);
  const double radius = number_prop(node, "radius", -1) < 0 ? em(0.8) : number_prop(node, "radius", 0);
  if (card && flag_prop(node, "glass", false)) {
    // Fake glass: translucent fill only. Real blur every frame is too expensive for 60 FPS UI.
  }
  if (raw_color > -2) {
    std::uint32_t fill = color_prop(node, "color", theme.panel);
    if (flag_prop(node, "glass", false) && raw_color < 0) {
      fill = (theme.panel & 0xFFFFFFU) | 0x50000000U;  // translucent over the blurred background
    }
    if (card && flag_prop(node, "shadow", true)) {
      fill_shadow(box.x, box.y + em(0.5), box.w, box.h, radius, em(1.5), theme.shadow);
    }
    fill_round_rect(box.x, box.y, box.w, box.h, radius, fill);
    if (flag_prop(node, "border", card)) {
      stroke_round_rect(box.x, box.y, box.w, box.h, radius, 1, theme.border);
    }
  }
  const std::string title = text_prop(node, "title");
  if (!title.empty()) {
    const double pad = padding_of(node);
    draw_label(title, box.x + pad, box.y + pad + (em(2.4) - text_height(ui_style().font)) / 2 - em(0.3), theme.text, 0, true);
    fill_rect(box.x + pad, box.y + pad + em(2.4) - em(0.6), box.w - 2 * pad, 1, theme.border);
  }
  set_clip(box.x, box.y, box.w, box.h);
  draw_children(node);
  clear_clip();
}

void draw(const Node& node) {
  ElementState& state = g_gui.states[node.id];
  const std::string appear = text_prop(node, "appear");
  const bool fading = (appear.find("fade") != std::string::npos || appear == "pop") && state.appear < 1.0;
  const int ax = static_cast<int>(std::floor(node.box.x - em(2)));
  const int ay = static_cast<int>(std::floor(node.box.y - em(2)));
  const int aw = static_cast<int>(std::ceil(node.box.w + em(4)));
  const int ah = static_cast<int>(std::ceil(node.box.h + em(4)));
  std::vector<std::uint32_t> before;
  if (fading) {
    before = read_area(ax, ay, aw, ah);
  }
  const Theme& theme = ui_style().theme;
  const Box& box = node.box;
  const Input& in = input();
  const double font = ui_style().font;
  const std::string text = text_prop(node, "text");
  if (container(node)) {
    draw_container(node);
  } else if (node.kind == "text") {
    const std::uint32_t fallback = flag_prop(node, "muted", false) ? theme.muted : theme.text;
    const double size = text_size(node);
    double x = box.x;
    const std::string align = text_prop(node, "align", "left");
    text::FontChoice& choice = text::choice();
    const bool was_bold = choice.bold;
    choice.bold = flag_prop(node, "bold", false);
    const double w = text_width(text, size);
    choice.bold = was_bold;
    if (align == "center") {
      x += (box.w - w) / 2;
    } else if (align == "right" || align == "end") {
      x += box.w - w;
    }
    draw_label(text, x, box.y, color_prop(node, "color", fallback), size, flag_prop(node, "bold", false));
  } else if (node.kind == "button") {
    if (press_released(node)) {
      g_gui.clicked.insert(node.id);
      g_gui.events.push_back(node.id);
    }
    const std::string style = text_prop(node, "style", "normal");
    const int kind = style == "primary" || style == "danger" ? 1 : style == "ghost" ? 2 : 0;
    const std::uint32_t accent = style == "danger" ? 0xE5484D : color_prop(node, "color", 0);
    draw_button(text, box, kind, node.id, g_gui.active == node.id && in.down[0], kind == 1 ? accent : 0);
  } else if (node.kind == "toggle" || node.kind == "checkbox") {
    if (!state.initialized) {
      state.on = flag_prop(node, "value", false);
      state.initialized = true;
    }
    if (press_released(node)) {
      state.on = !state.on;
      mark_changed(node.id);
    }
    draw_toggle(text, box, state.on, node.kind == "toggle", node.id);
  } else if (node.kind == "slider") {
    const double low = number_prop(node, "min", 0);
    const double high = number_prop(node, "max", 100);
    if (!state.initialized) {
      state.value = number_prop(node, "value", low);
      state.initialized = true;
    }
    const double line = text_height(font);
    const Box hit{box.x, box.y + line, box.w, box.h - line};
    if (g_gui.hovered == node.id && in.down[0] && !in.was_down[0] && g_gui.active.empty()) {
      g_gui.active = node.id;
    }
    const Box track = slider_track(hit);
    if (g_gui.active == node.id && in.down[0] && track.w > 0 && high != low) {
      double value = low + (high - low) * std::clamp((in.mouse_x - track.x) / track.w, 0.0, 1.0);
      const double step = number_prop(node, "step", 0);
      if (step > 0) {
        value = low + std::round((value - low) / step) * step;
      }
      if (value != state.value) {
        state.value = value;
        mark_changed(node.id);
      }
    }
    draw_slider(text, box, state.value, low, high, node.id, g_gui.active == node.id);
  } else if (node.kind == "textbox") {
    if (!state.initialized) {
      state.text = text_prop(node, "value");
      state.initialized = true;
    }
    const double label = text.empty() ? 0 : text_height(font) + em(0.3);
    const Box field{box.x, box.y + label, box.w, box.h - label};
    if (in.down[0] && !in.was_down[0]) {
      if (g_gui.hovered == node.id) {
        g_gui.focused = node.id;
      } else if (g_gui.focused == node.id) {
        g_gui.focused.clear();
      }
    }
    const bool focused = g_gui.focused == node.id;
    if (focused) {
      const std::string before_edit = state.text;
      state.text += in.typed;
      if (key_pressed("backspace")) {
        pop_utf8(state.text);
      }
      if (key_pressed("enter")) {
        g_gui.clicked.insert(node.id);  // Enter "submits" the box
        g_gui.events.push_back(node.id);
        g_gui.focused.clear();
      } else if (key_pressed("escape") || key_pressed("tab")) {
        g_gui.focused.clear();
      }
      if (state.text != before_edit) {
        mark_changed(node.id);
      }
    }
    if (!text.empty()) {
      draw_label(text, box.x, box.y, theme.text, 0, false);
    }
    draw_text_field(field, state.text, text_prop(node, "placeholder"), focused, node.id);
  } else if (node.kind == "choice") {
    const std::vector<std::string> options = split_options(text_prop(node, "options", "A|B"));
    if (!state.initialized) {
      state.selected = std::clamp(static_cast<int>(number_prop(node, "value", 0)), 0, static_cast<int>(options.size()) - 1);
      state.initialized = true;
    }
    const double label = text.empty() ? 0 : text_height(font) + em(0.3);
    const Box bar{box.x, box.y + label, box.w, box.h - label};
    if (press_released(node)) {
      const double cell = bar.w / static_cast<double>(options.size());
      const int picked = std::clamp(static_cast<int>((in.mouse_x - bar.x) / cell), 0, static_cast<int>(options.size()) - 1);
      if (picked != state.selected) {
        state.selected = picked;
        mark_changed(node.id);
      }
    }
    if (!text.empty()) {
      draw_label(text, box.x, box.y, theme.text, 0, false);
    }
    draw_segments(bar, options, state.selected, node.id);
  } else if (node.kind == "progress") {
    draw_progress(text, box, number_prop(node, "value", 0), color_prop(node, "color", theme.accent));
  } else if (node.kind == "image") {
    const int handle = static_cast<int>(number_prop(node, "image", 0));
    std::string ignored;
    const int resolved = handle != 0 ? handle : text_prop(node, "path").empty() ? 0 : load_image_file(text_prop(node, "path"), ignored);
    if (resolved != 0) {
      draw_image_box(resolved, box.x, box.y, box.w, box.h);
    } else {
      fill_round_rect(box.x, box.y, std::max(box.w, em(3)), std::max(box.h, em(3)), em(0.4), theme.widget);
    }
  } else if (node.kind == "divider") {
    fill_rect(box.x, std::round(box.y + box.h / 2), box.w, 1, theme.border);
  }
  if (fading) {
    const double eased = 1.0 - std::pow(1.0 - state.appear, 2.0);
    blend_area(ax, ay, aw, ah, before, eased);
  }
}

// --- natives -----------------------------------------------------------------------------------

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(gui_render) {
  (void)out;
  (void)arity;
  if (canvas().width <= 0 || canvas().height <= 0) {
    return fail(error, "Gui.Render: no canvas: call Window.Open(title, width, height) or Gfx.Canvas(width, height) first");
  }
  Node root;
  if (!build(args[0], root, "", 0, error)) {
    return false;
  }
  const Input& in = input();
  g_gui.frame = ++g_gui.renders;  // one Render per frame
  g_gui.events.clear();
  g_gui.clicked.clear();
  g_gui.changed.clear();
  if (!in.was_down[0]) {
    g_gui.active.clear();
  }
  const Box screen{0, 0, static_cast<double>(canvas().width), static_cast<double>(canvas().height)};
  if (root.kind != "screen") {  // a single element: put it on a screen of its own
    Node wrapper;
    wrapper.kind = "screen";
    wrapper.id = "screen";
    wrapper.children.push_back(std::move(root));
    root = std::move(wrapper);
  }
  measure(root, screen.w);
  root.width = screen.w;
  root.height = screen.h;
  place(root, screen);
  std::vector<const Node*> order;
  collect_hits(root, order);
  g_gui.hovered.clear();
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (mouse_in((*it)->box)) {
      if (interactive(**it)) {
        g_gui.hovered = (*it)->id;
      }
      break;  // the topmost element under the mouse wins, even a plain frame
    }
  }
  draw(root);
  return true;
}

CLPP_NATIVE(gui_query) {  // (id, which): 0 clicked, 1 changed, 2 hovered, 3 pressed, 4 focused
  if (arity != 2 || !args[0].is_string()) {
    return fail(error, "Gui: expected an element id (string)");
  }
  const std::string& id = args[0].text;
  bool result = false;
  switch (static_cast<int>(num(args[1]))) {
    case 0:
      result = g_gui.clicked.count(id) != 0;
      break;
    case 1:
      result = g_gui.changed.count(id) != 0;
      break;
    case 2:
      result = g_gui.hovered == id;
      break;
    case 3:
      result = g_gui.active == id && input().down[0];
      break;
    default:
      result = g_gui.focused == id;
      break;
  }
  out = boolean(result);
  return true;
}

CLPP_NATIVE(gui_events) {
  (void)args;
  (void)arity;
  (void)error;
  std::vector<Value> ids;
  for (const std::string& id : g_gui.events) {
    ids.push_back(Value::string_of(id));
  }
  out = Value::struct_of(std::move(ids));
  out.number = 0;
  return true;
}

CLPP_NATIVE(gui_get) {  // (id, which): 0 value, 1 on, 2 text, 3 selected
  if (arity != 2 || !args[0].is_string()) {
    return fail(error, "Gui: expected an element id (string)");
  }
  const auto found = g_gui.states.find(args[0].text);
  const ElementState state = found == g_gui.states.end() ? ElementState{} : found->second;
  switch (static_cast<int>(num(args[1]))) {
    case 0:
      out = Value::number_of(state.value);
      break;
    case 1:
      out = boolean(state.on);
      break;
    case 2:
      out = Value::string_of(state.text);
      break;
    default:
      out = Value::number_of(state.selected);
      break;
  }
  return true;
}

CLPP_NATIVE(gui_set) {  // (id, which, value)
  (void)out;
  if (arity != 3 || !args[0].is_string()) {
    return fail(error, "Gui: expected an element id (string)");
  }
  ElementState& state = g_gui.states[args[0].text];
  switch (static_cast<int>(num(args[1]))) {
    case 0:
      state.value = num(args[2]);
      break;
    case 1:
      state.on = num(args[2]) != 0;
      break;
    case 2:
      state.text = args[2].is_string() ? args[2].text : std::string{};
      break;
    default:
      state.selected = static_cast<int>(num(args[2]));
      break;
  }
  state.initialized = true;  // keep the new value instead of the element's starting one
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kGuiSource = R"clp(<< @clpp.gui: declarative interfaces (Roblox Fusion/Roact style). Describe the tree with named
<< properties, call Render every frame, then ask what happened: Clicked, Changed, Value, IsOn, GetText.
<< Interactive elements keep their state by `id` between frames; `value:` is only the starting value.
<< anchor: "top-left", "top", "top-right", "left", "center", "right", "bottom-left", "bottom", "bottom-right".
<< Colors: -1 = theme color, -2 = no fill (rows and columns are transparent by default).

func Screen(int color = -1, children = list()) {
  return list("kind", "screen", "color", color, "children", children);
}
func Frame(string id = "", string title = "", string anchor = "", float x = 0, float y = 0, float width = 0, float height = 0, string layout = "vertical", string align = "stretch", float padding = -1, float gap = -1, int color = -1, float radius = -1, bool shadow = true, bool border = true, bool glass = false, string appear = "", bool visible = true, children = list()) {
  return list("kind", "frame", "id", id, "title", title, "anchor", anchor, "anchor_given", anchor != "", "x", x, "y", y, "width", width, "height", height, "layout", layout, "align", align, "padding", padding, "gap", gap, "color", color, "radius", radius, "shadow", shadow, "border", border, "glass", glass, "appear", appear, "visible", visible, "children", children);
}
func Row(string id = "", float gap = -1, string align = "start", float height = 0, float padding = 0, int color = -2, bool visible = true, children = list()) {
  return list("kind", "row", "id", id, "gap", gap, "align", align, "height", height, "padding", padding, "color", color, "shadow", false, "border", false, "visible", visible, "children", children);
}
func Column(string id = "", float gap = -1, string align = "stretch", float width = 0, float padding = 0, int color = -2, bool visible = true, children = list()) {
  return list("kind", "column", "id", id, "gap", gap, "align", align, "width", width, "padding", padding, "color", color, "shadow", false, "border", false, "visible", visible, "children", children);
}
func Text(string text = "", float size = 0, int color = -1, bool bold = false, bool muted = false, string align = "left", string id = "", string anchor = "", float x = 0, float y = 0, string appear = "", bool visible = true) {
  return list("kind", "text", "text", text, "size", size, "color", color, "bold", bold, "muted", muted, "align", align, "id", id, "anchor", anchor, "x", x, "y", y, "appear", appear, "visible", visible);
}
func Button(string id = "", string text = "Button", string style = "normal", int color = -1, float width = 0, float height = 0, bool grow = false, string anchor = "", float x = 0, float y = 0, string appear = "", bool visible = true) {
  return list("kind", "button", "id", id, "text", text, "style", style, "color", color, "width", width, "height", height, "grow", grow, "anchor", anchor, "x", x, "y", y, "appear", appear, "visible", visible);
}
func Toggle(string id = "", string text = "", bool value = false, bool visible = true) {
  return list("kind", "toggle", "id", id, "text", text, "value", value, "visible", visible);
}
func Checkbox(string id = "", string text = "", bool value = false, bool visible = true) {
  return list("kind", "checkbox", "id", id, "text", text, "value", value, "visible", visible);
}
func Slider(string id = "", string text = "", float min = 0, float max = 100, float value = 0, float step = 0, float width = 0, bool visible = true) {
  return list("kind", "slider", "id", id, "text", text, "min", min, "max", max, "value", value, "step", step, "width", width, "visible", visible);
}
func TextBox(string id = "", string text = "", string placeholder = "", string value = "", float width = 0, bool visible = true) {
  return list("kind", "textbox", "id", id, "text", text, "placeholder", placeholder, "value", value, "width", width, "visible", visible);
}
func Choice(string id = "", string text = "", string options = "A|B", int value = 0, float width = 0, bool visible = true) {
  return list("kind", "choice", "id", id, "text", text, "options", options, "value", value, "width", width, "visible", visible);
}
func Progress(string text = "", float value = 0, int color = -1, float width = 0, string id = "", bool visible = true) {
  return list("kind", "progress", "text", text, "value", value, "color", color, "width", width, "id", id, "visible", visible);
}
func Image(string path = "", int image = 0, float width = 0, float height = 0, string anchor = "", float x = 0, float y = 0, string id = "", bool visible = true) {
  return list("kind", "image", "path", path, "image", image, "width", width, "height", height, "anchor", anchor, "x", x, "y", y, "id", id, "visible", visible);
}
func Spacer(float size = 12) {
  return list("kind", "spacer", "size", size);
}
func Divider() {
  return list("kind", "divider");
}

func Render(root) { gui::Render(root); }
func Clicked(string id) -> bool { return gui::Query(id, 0); }
func Changed(string id) -> bool { return gui::Query(id, 1); }
func Hovered(string id) -> bool { return gui::Query(id, 2); }
func Pressed(string id) -> bool { return gui::Query(id, 3); }
func Focused(string id) -> bool { return gui::Query(id, 4); }
func Events() { return gui::Events(); }
func Value(string id) -> float { return gui::Get(id, 0); }
func IsOn(string id) -> bool { return gui::Get(id, 1); }
func GetText(string id) -> string { return gui::Get(id, 2); }
func Selected(string id) -> int { return gui::Get(id, 3); }
func SetValue(string id, float value) { gui::Set(id, 0, value); }
func SetOn(string id, bool on) { gui::Set(id, 1, on); }
func SetText(string id, string text) { gui::Set(id, 2, text); }
func SetSelected(string id, int index) { gui::Set(id, 3, index); }
)clp";

}  // namespace

void add_gui(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"gui::Render", 1, gui_render, false},
                                {"gui::Query", 2, gui_query, false},
                                {"gui::Events", 0, gui_events, false},
                                {"gui::Get", 2, gui_get, false},
                                {"gui::Set", 3, gui_set, false},
                            });
}

std::string_view gui_source() { return kGuiSource; }

}  // namespace clpp::stdlib::host
