#include "stdlib/ui_style.hpp"

#include "stdlib/gfx_text.hpp"
#include "stdlib/host.hpp"

#include <algorithm>
#include <cmath>

namespace clpp::stdlib::host {

namespace {

constexpr Theme kDark{0x111118, 0x1C1C26, 0x22222E, 0xEDEDF5, 0x8C8CA3, 0x2A2A38, 0x34344A,
                      0x33334A, 0x7C5CFF, 0xFFFFFF, 0x80000000};
constexpr Theme kLight{0xF0F0F5, 0xFFFFFF, 0xF6F6FA, 0x1B1B26, 0x6E6E85, 0xF0F0F6, 0xE6E6F0,
                       0xDCDCE8, 0x5B3FE0, 0xFFFFFF, 0xC8303050};

[[nodiscard]] double em(const double factor) { return std::round(ui_style().font * factor); }

[[nodiscard]] std::uint32_t with_alpha(const std::uint32_t color, const double alpha) {
  const auto transparency = static_cast<std::uint32_t>(std::lround((1.0 - std::clamp(alpha, 0.0, 1.0)) * 255.0));
  return (color & 0xFFFFFFU) | (transparency << 24U);
}

}  // namespace

Style& ui_style() {
  static Style style = [] {
    Style initial;
    initial.theme = kDark;
    return initial;
  }();
  return style;
}

bool set_theme(const std::string& name, std::string& error) {
  Style& style = ui_style();
  const std::uint32_t accent = style.theme.accent;
  if (name == "dark") {
    style.theme = kDark;
  } else if (name == "light") {
    style.theme = kLight;
  } else {
    error = "unknown theme \"" + name + "\" (use \"dark\" or \"light\")";
    return false;
  }
  if (style.custom_accent) {
    style.theme.accent = accent;  // a chosen accent survives a theme change
  }
  return true;
}

void set_accent(const std::uint32_t color) {
  ui_style().theme.accent = color & 0xFFFFFFU;
  ui_style().custom_accent = true;
}

bool mouse_in(const Box& box) {
  const Input& in = input();
  return in.mouse_x >= box.x && in.mouse_x < box.x + box.w && in.mouse_y >= box.y && in.mouse_y < box.y + box.h;
}

double animate(std::unordered_map<std::string, double>& values, const std::string& id, const double target) {
  const auto found = values.find(id);
  if (found == values.end()) {
    values[id] = target;
    return target;
  }
  const double delta = input().delta;
  const double step = delta <= 0 ? 1.0 : std::min(1.0, delta * 12.0);
  found->second += (target - found->second) * step;
  if (std::fabs(found->second - target) < 0.01) {
    found->second = target;
  }
  return found->second;
}

std::vector<std::string> split_options(const std::string& text) {
  std::vector<std::string> options;
  std::size_t start = 0;
  while (true) {
    const std::size_t bar = text.find('|', start);
    options.push_back(text.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
    if (bar == std::string::npos) {
      break;
    }
    start = bar + 1;
  }
  return options;
}

void draw_card(const Box& box, const std::uint32_t color, const double radius, const bool shadow) {
  const Theme& theme = ui_style().theme;
  if (shadow) {
    fill_shadow(box.x, box.y + em(0.5), box.w, box.h, radius, em(1.5), theme.shadow);
  }
  fill_round_rect(box.x, box.y, box.w, box.h, radius, color);
  stroke_round_rect(box.x, box.y, box.w, box.h, radius, 1, theme.border);
}

void draw_label(const std::string_view text, const double x, const double y, const std::uint32_t color, const double size,
                const bool bold) {
  text::FontChoice& font = text::choice();
  const bool was_bold = font.bold;
  font.bold = bold || was_bold;
  draw_text(text, std::round(x), std::round(y), size > 0 ? size : ui_style().font, color);
  font.bold = was_bold;
}

void draw_button(const std::string_view text, const Box& box, const int style, const std::string& id, const bool pressed,
                 const std::uint32_t accent) {
  const Theme& theme = ui_style().theme;
  const std::uint32_t tone = accent != 0 ? accent : theme.accent;
  const double hover = animate(ui_style().hover, id, mouse_in(box) ? 1.0 : 0.0);
  const double radius = em(0.5);
  std::uint32_t fill = 0;
  std::uint32_t ink = theme.text;
  if (style == 1) {
    fill = mix_color(tone, 0xFFFFFF, 0.12 * hover);
    ink = theme.accent_text;
  } else if (style == 2) {
    fill = with_alpha(theme.hover, hover);
  } else {
    fill = mix_color(theme.widget, theme.hover, hover);
  }
  if (pressed) {
    fill = mix_color(fill, 0x000000, 0.18);
  }
  const double lift = pressed ? 1.0 : 0.0;
  if (style == 1) {
    fill_shadow(box.x, box.y + em(0.25), box.w, box.h, radius, em(0.7), with_alpha(tone, 0.25 + 0.2 * hover));
  }
  fill_round_rect(box.x, box.y + lift, box.w, box.h, radius, fill);
  if (style == 0) {
    stroke_round_rect(box.x, box.y + lift, box.w, box.h, radius, 1, theme.border);
  }
  const double size = ui_style().font;
  draw_label(text, box.x + (box.w - text_width(text, size)) / 2.0, box.y + (box.h - text_height(size)) / 2.0 + lift, ink,
             size, style == 1);
}

void draw_toggle(const std::string_view text, const Box& box, const bool value, const bool as_switch, const std::string& id) {
  const Theme& theme = ui_style().theme;
  const double hover = animate(ui_style().hover, id, mouse_in(box) ? 1.0 : 0.0);
  const double on = animate(ui_style().knob, id, value ? 1.0 : 0.0);
  const std::uint32_t idle = mix_color(theme.widget, theme.hover, hover);
  double text_x = box.x;
  if (as_switch) {
    const double h = em(1.35);
    const double w = em(2.4);
    const Box track{box.x, std::round(box.y + (box.h - h) / 2), w, h};
    fill_round_rect(track.x, track.y, track.w, track.h, h / 2, mix_color(idle, theme.accent, on));
    stroke_round_rect(track.x, track.y, track.w, track.h, h / 2, 1, mix_color(theme.border, theme.accent, on));
    const double knob = h - em(0.4);
    const double kx = track.x + em(0.2) + (track.w - knob - em(0.4)) * on;
    fill_shadow(kx, track.y + em(0.2) + 1, knob, knob, knob / 2, em(0.3), 0xB0000000);
    fill_circle(kx + knob / 2, track.y + h / 2, knob / 2, 0xFFFFFF);
    text_x = track.x + track.w + em(0.7);
  } else {
    const double size = em(1.25);
    const Box square{box.x, std::round(box.y + (box.h - size) / 2), size, size};
    fill_round_rect(square.x, square.y, size, size, em(0.3), mix_color(idle, theme.accent, on));
    stroke_round_rect(square.x, square.y, size, size, em(0.3), 1, mix_color(theme.border, theme.accent, on));
    if (on > 0.05) {
      const std::uint32_t ink = with_alpha(theme.accent_text, on);
      const double s = size / 10.0;
      const double thick = std::max(1.5, s * 1.4);
      stroke_line(square.x + 2.6 * s, square.y + 5.2 * s, square.x + 4.3 * s, square.y + 7.0 * s, thick, ink);
      stroke_line(square.x + 4.3 * s, square.y + 7.0 * s, square.x + 7.6 * s, square.y + 3.2 * s, thick, ink);
    }
    text_x = square.x + size + em(0.6);
  }
  const double font = ui_style().font;
  draw_label(text, text_x, box.y + (box.h - text_height(font)) / 2.0, theme.text, font, false);
}

Box slider_track(const Box& hit) {
  const double knob = em(0.7);
  return Box{hit.x + knob, hit.y + hit.h / 2 - em(0.15), hit.w - 2 * knob, em(0.3)};
}

void draw_slider(const std::string_view text, const Box& box, const double value, const double low, const double high,
                 const std::string& id, const bool dragging) {
  const Theme& theme = ui_style().theme;
  const double font = ui_style().font;
  const double line = text_height(font);
  const Box hit{box.x, box.y + line, box.w, box.h - line};
  const Box track = slider_track(hit);
  const double hover = animate(ui_style().hover, id, mouse_in(hit) || dragging ? 1.0 : 0.0);
  const double t = high == low ? 0 : std::clamp((value - low) / (high - low), 0.0, 1.0);
  draw_label(text, box.x, box.y, theme.text, font, false);
  double shown = std::fabs(high - low) >= 10 ? std::round(value) : std::round(value * 100) / 100;
  std::string number = std::to_string(shown);
  number.erase(number.find_last_not_of('0') + 1);
  if (!number.empty() && number.back() == '.') {
    number.pop_back();
  }
  draw_label(number, box.x + box.w - text_width(number, font), box.y, theme.muted, font, false);
  fill_round_rect(track.x, track.y, track.w, track.h, track.h / 2, theme.hover);
  fill_round_rect(track.x, track.y, std::max(track.h, track.w * t), track.h, track.h / 2, theme.accent);
  const double knob = em(0.7);
  const double cx = track.x + track.w * t;
  const double cy = track.y + track.h / 2;
  fill_circle(cx, cy, knob + em(0.35) * hover, with_alpha(theme.accent, 0.25));
  fill_shadow(cx - knob, cy - knob + 1, knob * 2, knob * 2, knob, em(0.4), 0xA0000000);
  fill_circle(cx, cy, knob, 0xFFFFFF);
}

void draw_progress(const std::string_view text, const Box& box, const double fraction, const std::uint32_t color) {
  const Theme& theme = ui_style().theme;
  const double font = ui_style().font;
  const double t = std::clamp(fraction, 0.0, 1.0);
  double top = box.y;
  if (!text.empty()) {
    draw_label(text, box.x, box.y, theme.text, font, false);
    const std::string percent = std::to_string(static_cast<int>(std::lround(t * 100))) + "%";
    draw_label(percent, box.x + box.w - text_width(percent, font), box.y, theme.muted, font, false);
    top += text_height(font);
  }
  const Box bar{box.x, top + em(0.1), box.w, em(0.45)};
  fill_round_rect(bar.x, bar.y, bar.w, bar.h, bar.h / 2, theme.hover);
  if (t > 0) {
    fill_round_rect(bar.x, bar.y, std::max(bar.h, bar.w * t), bar.h, bar.h / 2, color);
  }
}

void draw_text_field(const Box& field, const std::string& value, const std::string& placeholder, const bool focused,
                     const std::string& id) {
  const Theme& theme = ui_style().theme;
  const double font = ui_style().font;
  const double hover = animate(ui_style().hover, id, focused ? 1.0 : mouse_in(field) ? 0.5 : 0.0);
  const double radius = em(0.5);
  if (focused) {
    fill_round_rect(field.x - 3, field.y - 3, field.w + 6, field.h + 6, radius + 3, with_alpha(theme.accent, 0.3));
  }
  fill_round_rect(field.x, field.y, field.w, field.h, radius, theme.widget);
  stroke_round_rect(field.x, field.y, field.w, field.h, radius, 1, mix_color(theme.border, theme.accent, hover));
  std::string visible = value;  // show the end of long text
  const double room = field.w - 2 * em(0.7);
  while (!visible.empty() && text_width(visible, font) > room) {
    std::size_t cut = 1;
    while (cut < visible.size() && (static_cast<unsigned char>(visible[cut]) & 0xC0U) == 0x80U) {
      ++cut;
    }
    visible.erase(0, cut);
  }
  const double text_y = field.y + (field.h - text_height(font)) / 2.0;
  if (visible.empty() && !focused) {
    draw_label(placeholder, field.x + em(0.7), text_y, theme.muted, font, false);
  } else {
    draw_label(visible, field.x + em(0.7), text_y, theme.text, font, false);
  }
  if (focused && (input().frame / 30) % 2 == 0) {
    const double caret = field.x + em(0.7) + text_width(visible, font) + 1;
    fill_rect(std::round(caret), field.y + em(0.55), std::max(1.0, em(0.1)), field.h - em(1.1), theme.accent);
  }
}

void draw_segments(const Box& bar, const std::vector<std::string>& options, const int selected, const std::string& id) {
  const Theme& theme = ui_style().theme;
  const double font = ui_style().font;
  const double radius = em(0.5);
  fill_round_rect(bar.x, bar.y, bar.w, bar.h, radius, theme.widget);
  stroke_round_rect(bar.x, bar.y, bar.w, bar.h, radius, 1, theme.border);
  const double cell = bar.w / static_cast<double>(std::max<std::size_t>(1, options.size()));
  const double position = animate(ui_style().knob, id, selected);  // the highlight slides
  fill_round_rect(bar.x + cell * position + em(0.2), bar.y + em(0.2), cell - em(0.4), bar.h - em(0.4), radius - em(0.1),
                  theme.accent);
  for (std::size_t index = 0; index < options.size(); ++index) {
    const double x = bar.x + cell * static_cast<double>(index);
    const bool chosen = std::fabs(position - static_cast<double>(index)) < 0.5;
    draw_label(options[index], x + (cell - text_width(options[index], font)) / 2, bar.y + (bar.h - text_height(font)) / 2,
               chosen ? theme.accent_text : theme.text, font, chosen);
  }
}

}  // namespace clpp::stdlib::host
