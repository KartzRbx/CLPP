#pragma once

// The built-in bitmap font of @clpp.gfx: 5x7 glyphs (plus 2 descender rows) for printable ASCII,
// accents composed on top for Latin-1 letters (á, ç, ã, ü...), and a few typographic symbols
// mapped to the closest glyph. Drawn by software, so text looks the same in a window, off screen
// and in a saved PNG, on every platform.

#include <cstdint>
#include <string_view>

namespace clpp::stdlib::font {

inline constexpr int kGlyphWidth = 5;
inline constexpr int kAdvance = 6;      // glyph + 1 column of spacing
inline constexpr int kLineHeight = 12;  // 2 accent rows + 7 + 2 descender rows + 1 spacing
inline constexpr int kBaseRow = 2;      // row 0 of a glyph is row 2 of the line

// 9 rows of 5 columns each, '#' = ink. Rows 7 and 8 hold descenders.
using Rows = const char* const*;

enum class Accent : std::uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Ring, Cedilla };

struct Glyph {
  Rows rows{nullptr};
  Accent accent{Accent::None};
  bool lowercase{false};  // accents sit lower on x-height letters
  bool dotless{false};    // í: draw the i without its dot
};

// Calls `emit(glyph)` for every glyph of the UTF-8 text, and `newline()` at each '\n'.
template <typename Emit, typename Newline>
void each_glyph(std::string_view text, Emit&& emit, Newline&& newline);

[[nodiscard]] Glyph glyph_for(std::uint32_t codepoint);

[[nodiscard]] Rows accent_rows(Accent accent);

}  // namespace clpp::stdlib::font

namespace clpp::stdlib::font {

[[nodiscard]] inline std::uint32_t next_codepoint(std::string_view text, std::size_t& index) {
  const auto lead = static_cast<unsigned char>(text[index]);
  std::size_t width = 1;
  std::uint32_t code = lead;
  if (lead >= 0xF0) {
    width = 4;
    code = lead & 0x07U;
  } else if (lead >= 0xE0) {
    width = 3;
    code = lead & 0x0FU;
  } else if (lead >= 0xC0) {
    width = 2;
    code = lead & 0x1FU;
  }
  if (index + width > text.size()) {
    index = text.size();
    return 0xFFFD;
  }
  for (std::size_t extra = 1; extra < width; ++extra) {
    code = (code << 6U) | (static_cast<unsigned char>(text[index + extra]) & 0x3FU);
  }
  index += width;
  return code;
}

template <typename Emit, typename Newline>
void each_glyph(const std::string_view text, Emit&& emit, Newline&& newline) {
  std::size_t index = 0;
  while (index < text.size()) {
    const std::uint32_t code = next_codepoint(text, index);
    if (code == '\n') {
      newline();
      continue;
    }
    if (code == '\r') {
      continue;
    }
    if (code == '\t') {
      for (int spaces = 0; spaces < 2; ++spaces) {
        emit(glyph_for(' '));
      }
      continue;
    }
    if (code == 0x2026) {  // … as three dots
      for (int dots = 0; dots < 3; ++dots) {
        emit(glyph_for('.'));
      }
      continue;
    }
    emit(glyph_for(code));
  }
}

}  // namespace clpp::stdlib::font
