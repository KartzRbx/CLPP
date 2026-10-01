#pragma once

// Look and drawing of the widgets shared by @clpp.ui (immediate mode) and @clpp.gui (declarative):
// one theme, one font size, one set of animations, so both libraries look the same.

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clpp::stdlib::host {

struct Theme {
  std::uint32_t background;
  std::uint32_t panel;
  std::uint32_t title;
  std::uint32_t text;
  std::uint32_t muted;
  std::uint32_t widget;
  std::uint32_t hover;
  std::uint32_t border;
  std::uint32_t accent;
  std::uint32_t accent_text;
  std::uint32_t shadow;
};

struct Style {
  Theme theme{};
  double font{15};
  bool custom_accent{false};
  std::unordered_map<std::string, double> hover;  // animated 0..1 per widget id
  std::unordered_map<std::string, double> knob;   // animated positions (toggles, segments)
};

struct Box {
  double x;
  double y;
  double w;
  double h;
};

[[nodiscard]] Style& ui_style();
bool set_theme(const std::string& name, std::string& error);
void set_accent(std::uint32_t color);

[[nodiscard]] bool mouse_in(const Box& box);
// Eases a per-widget value toward `target` (about 120 ms); without frames it jumps there.
[[nodiscard]] double animate(std::unordered_map<std::string, double>& values, const std::string& id, double target);
[[nodiscard]] std::vector<std::string> split_options(const std::string& text);

void draw_card(const Box& box, std::uint32_t color, double radius, bool shadow);
void draw_label(std::string_view text, double x, double y, std::uint32_t color, double size, bool bold);
// style: 0 normal, 1 primary (accent), 2 ghost (no fill until hovered)
void draw_button(std::string_view text, const Box& box, int style, const std::string& id, bool pressed,
                 std::uint32_t accent = 0);
void draw_toggle(std::string_view text, const Box& box, bool value, bool as_switch, const std::string& id);
[[nodiscard]] Box slider_track(const Box& hit);
void draw_slider(std::string_view text, const Box& box, double value, double low, double high, const std::string& id,
                 bool dragging);
void draw_progress(std::string_view text, const Box& box, double fraction, std::uint32_t color);
void draw_text_field(const Box& field, const std::string& value, const std::string& placeholder, bool focused,
                     const std::string& id);
void draw_segments(const Box& bar, const std::vector<std::string>& options, int selected, const std::string& id);

}  // namespace clpp::stdlib::host
