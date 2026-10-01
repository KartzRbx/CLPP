// Text for @clpp.gfx: smooth TrueType text by default, the built-in pixel font on request.
//
// On Windows glyphs come from the system fonts through GDI (GetGlyphOutline with 65 levels of
// gray), are cached per font, weight, size and character, and are blended onto the canvas, so
// "Segoe UI" text looks like any modern Windows app. Elsewhere, or with Gfx.Font("pixel"), the
// built-in 5x7 bitmap font is used (crisp, retro, identical on every system).

#include "stdlib/gfx_font.hpp"
#include "stdlib/gfx_text.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace clpp::stdlib::text {

namespace {

FontChoice g_choice;
std::mutex g_mutex;

[[nodiscard]] int pixel_scale(const double size) {
  return std::max(1, static_cast<int>(std::lround(size / font::kLineHeight)));
}

#ifdef _WIN32

struct Face {
  HFONT font{nullptr};
  int ascent{0};
  int line_height{0};
  std::unordered_map<std::uint32_t, Glyph> glyphs;
};

[[nodiscard]] std::wstring widen(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
  return wide;
}

HDC memory_dc() {
  static HDC dc = CreateCompatibleDC(nullptr);
  return dc;
}

// One face per (family, bold, pixel size); never freed (a program uses a handful).
Face* face_for(const FontChoice& choice, const int size) {
  static std::map<std::tuple<std::string, bool, int>, std::unique_ptr<Face>> faces;
  const auto key = std::make_tuple(choice.family, choice.bold, size);
  const auto found = faces.find(key);
  if (found != faces.end()) {
    return found->second.get();
  }
  auto face = std::make_unique<Face>();
  const std::wstring family = widen(choice.family);
  face->font = CreateFontW(-size, 0, 0, 0, choice.bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           family.c_str());
  if (face->font == nullptr) {
    faces.emplace(key, nullptr);
    return nullptr;
  }
  SelectObject(memory_dc(), face->font);
  TEXTMETRICW metrics{};
  GetTextMetricsW(memory_dc(), &metrics);
  face->ascent = metrics.tmAscent;
  face->line_height = metrics.tmHeight;
  Face* result = face.get();
  faces.emplace(key, std::move(face));
  return result;
}

const Glyph& glyph_of(Face& face, const std::uint32_t code) {
  const auto found = face.glyphs.find(code);
  if (found != face.glyphs.end()) {
    return found->second;
  }
  Glyph glyph;
  SelectObject(memory_dc(), face.font);
  const UINT character = code <= 0xFFFF ? static_cast<UINT>(code) : static_cast<UINT>('?');
  GLYPHMETRICS metrics{};
  const MAT2 identity{{0, 1}, {0, 0}, {0, 0}, {0, 1}};
  const DWORD size = GetGlyphOutlineW(memory_dc(), character, GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &identity);
  glyph.advance = metrics.gmCellIncX;
  if (size != GDI_ERROR && size > 0) {
    std::vector<std::uint8_t> buffer(size);
    if (GetGlyphOutlineW(memory_dc(), character, GGO_GRAY8_BITMAP, &metrics, size, buffer.data(), &identity) != GDI_ERROR) {
      glyph.width = static_cast<int>(metrics.gmBlackBoxX);
      glyph.height = static_cast<int>(metrics.gmBlackBoxY);
      glyph.left = metrics.gmptGlyphOrigin.x;
      glyph.top = face.ascent - metrics.gmptGlyphOrigin.y;
      const std::size_t pitch = (static_cast<std::size_t>(glyph.width) + 3U) & ~static_cast<std::size_t>(3U);
      glyph.alpha.resize(static_cast<std::size_t>(glyph.width) * static_cast<std::size_t>(glyph.height));
      for (int y = 0; y < glyph.height; ++y) {
        for (int x = 0; x < glyph.width; ++x) {
          const std::uint8_t level = buffer[static_cast<std::size_t>(y) * pitch + static_cast<std::size_t>(x)];
          glyph.alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(glyph.width) + static_cast<std::size_t>(x)] =
              static_cast<std::uint8_t>(std::min(255, level * 255 / 64));
        }
      }
    }
  } else if (size == GDI_ERROR) {
    glyph.advance = 0;
  }
  return face.glyphs.emplace(code, std::move(glyph)).first->second;
}

#endif  // _WIN32

void each_codepoint(const std::string_view text, const std::function<void(std::uint32_t)>& visit) {
  std::size_t index = 0;
  while (index < text.size()) {
    visit(font::next_codepoint(text, index));
  }
}

}  // namespace

FontChoice& choice() { return g_choice; }

bool smooth_available() {
#ifdef _WIN32
  return !g_choice.pixel;
#else
  return false;
#endif
}

double line_height(const double size) {
  if (!smooth_available()) {
    return font::kLineHeight * pixel_scale(size);
  }
#ifdef _WIN32
  const std::lock_guard<std::mutex> lock(g_mutex);
  if (Face* face = face_for(g_choice, std::max(4, static_cast<int>(std::lround(size))))) {
    return face->line_height;
  }
#endif
  return font::kLineHeight * pixel_scale(size);
}

double width(const std::string_view text, const double size) {
  if (!smooth_available()) {
    return static_cast<double>(pixel_width(text, pixel_scale(size)));
  }
#ifdef _WIN32
  const std::lock_guard<std::mutex> lock(g_mutex);
  Face* face = face_for(g_choice, std::max(4, static_cast<int>(std::lround(size))));
  if (face != nullptr) {
    double widest = 0;
    double current = 0;
    each_codepoint(text, [&](const std::uint32_t code) {
      if (code == '\n') {
        widest = std::max(widest, current);
        current = 0;
      } else if (code != '\r') {
        current += glyph_of(*face, code == '\t' ? ' ' : code).advance * (code == '\t' ? 4 : 1);
      }
    });
    return std::max(widest, current);
  }
#endif
  return static_cast<double>(pixel_width(text, pixel_scale(size)));
}

void draw(const std::string_view text, const double x, const double y, const double size, const BlendFn& blend) {
  if (!smooth_available()) {
    draw_pixel_font(text, x, y, pixel_scale(size), blend);
    return;
  }
#ifdef _WIN32
  const std::lock_guard<std::mutex> lock(g_mutex);
  Face* face = face_for(g_choice, std::max(4, static_cast<int>(std::lround(size))));
  if (face == nullptr) {
    draw_pixel_font(text, x, y, pixel_scale(size), blend);
    return;
  }
  double pen_x = std::round(x);
  double pen_y = std::round(y);
  each_codepoint(text, [&](const std::uint32_t code) {
    if (code == '\n') {
      pen_x = std::round(x);
      pen_y += face->line_height;
      return;
    }
    if (code == '\r') {
      return;
    }
    const Glyph& glyph = glyph_of(*face, code == '\t' ? ' ' : code);
    for (int gy = 0; gy < glyph.height; ++gy) {
      for (int gx = 0; gx < glyph.width; ++gx) {
        const std::uint8_t alpha =
            glyph.alpha[static_cast<std::size_t>(gy) * static_cast<std::size_t>(glyph.width) + static_cast<std::size_t>(gx)];
        if (alpha != 0) {
          blend(static_cast<int>(pen_x) + glyph.left + gx, static_cast<int>(pen_y) + glyph.top + gy, alpha / 255.0);
        }
      }
    }
    pen_x += glyph.advance * (code == '\t' ? 4 : 1);
  });
#endif
}

int pixel_width(const std::string_view text, const int scale) {
  const int s = std::max(1, scale);
  int widest = 0;
  int current = 0;
  font::each_glyph(
      text, [&](const font::Glyph&) { ++current; },
      [&] {
        widest = std::max(widest, current);
        current = 0;
      });
  widest = std::max(widest, current);
  return widest == 0 ? 0 : (widest * font::kAdvance - 1) * s;
}

void draw_pixel_font(const std::string_view text, const double x, const double y, const int scale, const BlendFn& blend) {
  const int s = std::max(1, std::min(scale, 64));
  double pen_x = std::round(x);
  double pen_y = std::round(y);
  const auto block = [&](const int column, const int row) {
    for (int dy = 0; dy < s; ++dy) {
      for (int dx = 0; dx < s; ++dx) {
        blend(static_cast<int>(pen_x) + column * s + dx, static_cast<int>(pen_y) + row * s + dy, 1.0);
      }
    }
  };
  const auto draw_rows = [&](font::Rows rows, const int count, const int first_row, const bool skip_dot) {
    for (int row = 0; row < count; ++row) {
      if (skip_dot && row < 2) {
        continue;
      }
      for (int column = 0; column < font::kGlyphWidth; ++column) {
        if (rows[row][column] == '#') {
          block(column, first_row + row);
        }
      }
    }
  };
  font::each_glyph(
      text,
      [&](const font::Glyph& glyph) {
        draw_rows(glyph.rows, 9, font::kBaseRow, glyph.dotless);
        if (const font::Rows accent = font::accent_rows(glyph.accent)) {
          const int accent_row = glyph.accent == font::Accent::Cedilla ? font::kBaseRow + 7
                                 : glyph.lowercase                     ? font::kBaseRow
                                                                       : 0;
          draw_rows(accent, 2, accent_row, false);
        }
        pen_x += font::kAdvance * s;
      },
      [&] {
        pen_x = std::round(x);
        pen_y += font::kLineHeight * s;
      });
}

}  // namespace clpp::stdlib::text
