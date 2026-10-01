#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::stdlib::text {

struct FontChoice {
  std::string family{"Segoe UI"};
  bool bold{false};
  bool pixel{false};  // the built-in 5x7 bitmap font
};

struct Glyph {
  int width{0};
  int height{0};
  int left{0};  // from the pen position
  int top{0};   // from the top of the line
  double advance{0};
  std::vector<std::uint8_t> alpha;  // width * height coverage, 0..255
};

// blend(x, y, coverage 0..1) paints one pixel of text.
using BlendFn = std::function<void(int, int, double)>;

[[nodiscard]] FontChoice& choice();
[[nodiscard]] bool smooth_available();

// `size` is the font size in pixels (like CSS font-size); `y` is the top of the line.
[[nodiscard]] double line_height(double size);
[[nodiscard]] double width(std::string_view text, double size);
void draw(std::string_view text, double x, double y, double size, const BlendFn& blend);

[[nodiscard]] int pixel_width(std::string_view text, int scale);
void draw_pixel_font(std::string_view text, double x, double y, int scale, const BlendFn& blend);

}  // namespace clpp::stdlib::text
