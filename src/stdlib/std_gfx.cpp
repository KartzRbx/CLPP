// @clpp.gfx — 2D drawing on a software canvas.
//
// Everything is drawn by the CPU into one 32-bit pixel buffer. @clpp.window shows that buffer in
// a native window; without a window the canvas still works off screen (servers, tests, tools that
// generate images), and Gfx.Save writes it as PNG or BMP. Shapes are anti-aliased with coverage
// computed from signed distances; text is smooth TrueType (gfx_text.cpp) with the built-in pixel
// font as an option; Gfx.Effect applies "shader" style post-processing to the canvas.
//
// Colors are ints: 0xRRGGBB. The top byte is transparency (0 = opaque, 255 = invisible), so a
// plain hex literal is an opaque color and Gfx.Fade/Gfx.Rgba produce translucent ones.

#include "stdlib/gfx_font.hpp"
#include "stdlib/gfx_text.hpp"
#include "stdlib/host.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clpp::stdlib::host {

namespace {

Canvas g_canvas;

struct Clip {
  int x0{0};
  int y0{0};
  int x1{1 << 30};
  int y1{1 << 30};
};
Clip g_clip;

struct Image {
  int width{0};
  int height{0};
  std::vector<std::uint32_t> pixels;  // 0xAARRGGBB with real alpha
};
std::vector<Image> g_images;  // handle = index + 1

struct Paint {
  std::uint32_t rgb{0};
  double alpha{1};
};

[[nodiscard]] std::uint32_t color_bits(const double value) {
  if (!std::isfinite(value)) {
    return 0;
  }
  return static_cast<std::uint32_t>(static_cast<std::int64_t>(value) & 0xFFFFFFFFLL);
}

[[nodiscard]] Paint paint_of(const std::uint32_t color) {
  const std::uint32_t transparency = (color >> 24U) & 0xFFU;
  return Paint{color & 0xFFFFFFU, static_cast<double>(255U - transparency) / 255.0};
}

[[nodiscard]] int clamp_int(const double value, const int low, const int high) {
  if (!(value > low)) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return static_cast<int>(value);
}

// Visible area: clip rectangle ∩ canvas.
[[nodiscard]] Clip bounds() {
  return Clip{std::max(0, g_clip.x0), std::max(0, g_clip.y0), std::min(g_canvas.width, g_clip.x1),
              std::min(g_canvas.height, g_clip.y1)};
}

void blend(std::uint32_t& dst, const std::uint32_t rgb, const double alpha) {
  if (alpha >= 0.998) {
    dst = 0xFF000000U | rgb;
    return;
  }
  if (alpha <= 0.002) {
    return;
  }
  const auto a = static_cast<std::uint32_t>(alpha * 256.0);
  const std::uint32_t inv = 256U - a;
  const std::uint32_t r = ((((rgb >> 16U) & 0xFFU) * a) + (((dst >> 16U) & 0xFFU) * inv)) >> 8U;
  const std::uint32_t g = ((((rgb >> 8U) & 0xFFU) * a) + (((dst >> 8U) & 0xFFU) * inv)) >> 8U;
  const std::uint32_t b = (((rgb & 0xFFU) * a) + ((dst & 0xFFU) * inv)) >> 8U;
  dst = 0xFF000000U | (r << 16U) | (g << 8U) | b;
}

void plot(const int x, const int y, const Paint& paint, const double coverage, const Clip& area) {
  if (x < area.x0 || y < area.y0 || x >= area.x1 || y >= area.y1) {
    return;
  }
  blend(g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                        static_cast<std::size_t>(x)],
        paint.rgb, paint.alpha * coverage);
}

[[nodiscard]] double saturate(const double value) { return value < 0 ? 0 : value > 1 ? 1 : value; }

[[nodiscard]] bool require_canvas(std::string& error) {
  if (g_canvas.width <= 0 || g_canvas.height <= 0) {
    error = "no canvas: call Gfx.Canvas(width, height) or Window.Open(title, width, height) first";
    return false;
  }
  return true;
}

// Distance from (px, py) to the segment a-b.
[[nodiscard]] double segment_distance(const double px, const double py, const double ax, const double ay,
                                      const double bx, const double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double length = dx * dx + dy * dy;
  double t = length > 0 ? ((px - ax) * dx + (py - ay) * dy) / length : 0;
  t = saturate(t);
  const double cx = ax + t * dx - px;
  const double cy = ay + t * dy - py;
  return std::sqrt(cx * cx + cy * cy);
}

void draw_line(const double x1, const double y1, const double x2, const double y2, const double thickness,
               const std::uint32_t color) {
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const double half = std::max(0.5, thickness / 2.0);
  const int left = clamp_int(std::floor(std::min(x1, x2) - half - 1), area.x0, area.x1);
  const int right = clamp_int(std::ceil(std::max(x1, x2) + half + 1), area.x0, area.x1);
  const int top = clamp_int(std::floor(std::min(y1, y2) - half - 1), area.y0, area.y1);
  const int bottom = clamp_int(std::ceil(std::max(y1, y2) + half + 1), area.y0, area.y1);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const double distance = segment_distance(x + 0.5, y + 0.5, x1, y1, x2, y2);
      const double coverage = saturate(half + 0.5 - distance);
      if (coverage > 0) {
        plot(x, y, paint, coverage, area);
      }
    }
  }
}

void draw_ring(const double cx, const double cy, const double r, const double thickness, const std::uint32_t color) {
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const double half = std::max(0.5, thickness / 2.0);
  const double reach = r + half + 1;
  const int left = clamp_int(std::floor(cx - reach), area.x0, area.x1);
  const int right = clamp_int(std::ceil(cx + reach), area.x0, area.x1);
  const int top = clamp_int(std::floor(cy - reach), area.y0, area.y1);
  const int bottom = clamp_int(std::ceil(cy + reach), area.y0, area.y1);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const double dx = x + 0.5 - cx;
      const double dy = y + 0.5 - cy;
      const double coverage = saturate(half + 0.5 - std::fabs(std::sqrt(dx * dx + dy * dy) - r));
      if (coverage > 0) {
        plot(x, y, paint, coverage, area);
      }
    }
  }
}

void fill_triangle(const double x1, const double y1, const double x2, const double y2, const double x3,
                   const double y3, const std::uint32_t color) {
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const auto edge = [](const double ax, const double ay, const double bx, const double by, const double px,
                       const double py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
  const double total = edge(x1, y1, x2, y2, x3, y3);
  if (std::fabs(total) < 1e-9) {
    return;
  }
  const int left = clamp_int(std::floor(std::min({x1, x2, x3})), area.x0, area.x1);
  const int right = clamp_int(std::ceil(std::max({x1, x2, x3})), area.x0, area.x1);
  const int top = clamp_int(std::floor(std::min({y1, y2, y3})), area.y0, area.y1);
  const int bottom = clamp_int(std::ceil(std::max({y1, y2, y3})), area.y0, area.y1);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const double px = x + 0.5;
      const double py = y + 0.5;
      const double w0 = edge(x2, y2, x3, y3, px, py);
      const double w1 = edge(x3, y3, x1, y1, px, py);
      const double w2 = edge(x1, y1, x2, y2, px, py);
      const bool inside = total > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
      if (inside) {
        plot(x, y, paint, 1.0, area);
      }
    }
  }
}

void fill_gradient(const double x, const double y, const double w, const double h, const std::uint32_t top,
                   const std::uint32_t bottom, const bool horizontal) {
  const Clip area = bounds();
  const int x0 = clamp_int(std::round(x), area.x0, area.x1);
  const int x1 = clamp_int(std::round(x + w), area.x0, area.x1);
  const int y0 = clamp_int(std::round(y), area.y0, area.y1);
  const int y1 = clamp_int(std::round(y + h), area.y0, area.y1);
  for (int py = y0; py < y1; ++py) {
    for (int px = x0; px < x1; ++px) {
      const double t = horizontal ? (w > 1 ? (px + 0.5 - x) / w : 0) : (h > 1 ? (py + 0.5 - y) / h : 0);
      plot(px, py, paint_of(mix_color(top, bottom, saturate(t))), 1.0, area);
    }
  }
}

// --- images -------------------------------------------------------------------------------------

[[nodiscard]] const Image* image_at(const double handle, std::string& error) {
  const auto index = static_cast<long long>(handle) - 1;
  if (index < 0 || static_cast<std::size_t>(index) >= g_images.size() || g_images[static_cast<std::size_t>(index)].width == 0) {
    error = "invalid image handle: " + std::to_string(static_cast<long long>(handle));
    return nullptr;
  }
  return &g_images[static_cast<std::size_t>(index)];
}

void draw_image(const Image& image, const double sx, const double sy, const double sw, const double sh, const double dx,
                const double dy, const double dw, const double dh) {
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
    return;
  }
  const Clip area = bounds();
  const int x0 = clamp_int(std::round(dx), area.x0, area.x1);
  const int x1 = clamp_int(std::round(dx + dw), area.x0, area.x1);
  const int y0 = clamp_int(std::round(dy), area.y0, area.y1);
  const int y1 = clamp_int(std::round(dy + dh), area.y0, area.y1);
  for (int y = y0; y < y1; ++y) {
    const int source_y = static_cast<int>(std::floor(sy + (y + 0.5 - dy) * sh / dh));
    if (source_y < 0 || source_y >= image.height) {
      continue;
    }
    for (int x = x0; x < x1; ++x) {
      const int source_x = static_cast<int>(std::floor(sx + (x + 0.5 - dx) * sw / dw));
      if (source_x < 0 || source_x >= image.width) {
        continue;
      }
      const std::uint32_t pixel = image.pixels[static_cast<std::size_t>(source_y) * static_cast<std::size_t>(image.width) +
                                               static_cast<std::size_t>(source_x)];
      const double alpha = static_cast<double>((pixel >> 24U) & 0xFFU) / 255.0;
      if (alpha > 0) {
        blend(g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                              static_cast<std::size_t>(x)],
              pixel & 0xFFFFFFU, alpha);
      }
    }
  }
}

// Minimal inflate (RFC 1951), enough for PNG IDAT streams.
class Inflater {
 public:
  explicit Inflater(const std::vector<std::uint8_t>& data) : m_data(data) {}

  bool run(std::vector<std::uint8_t>& out, std::string& error) {
    if (m_data.size() < 2 || (m_data[0] & 0x0FU) != 8) {
      error = "unsupported compression";
      return false;
    }
    m_pos = 2;  // zlib header
    bool last = false;
    while (!last) {
      last = bits(1) != 0;
      const int type = bits(2);
      bool ok = false;
      if (type == 0) {
        ok = stored(out);
      } else if (type == 1) {
        ok = fixed(out);
      } else if (type == 2) {
        ok = dynamic(out);
      }
      if (!ok || m_overrun) {
        error = "corrupt compressed data";
        return false;
      }
    }
    return true;
  }

 private:
  struct Huffman {
    std::array<std::uint16_t, 16> count{};
    std::vector<std::uint16_t> symbol;
  };

  int bit() {
    if (m_pos >= m_data.size()) {
      m_overrun = true;
      return 0;
    }
    const int value = (m_data[m_pos] >> m_bit) & 1;
    if (++m_bit == 8) {
      m_bit = 0;
      ++m_pos;
    }
    return value;
  }

  int bits(const int count) {
    int value = 0;
    for (int index = 0; index < count; ++index) {
      value |= bit() << index;
    }
    return value;
  }

  static void build(Huffman& table, const std::uint8_t* lengths, const int n) {
    table.count.fill(0);
    for (int symbol = 0; symbol < n; ++symbol) {
      ++table.count[lengths[symbol]];
    }
    std::array<std::uint16_t, 16> offsets{};
    for (int length = 1; length < 15; ++length) {
      offsets[static_cast<std::size_t>(length + 1)] =
          static_cast<std::uint16_t>(offsets[static_cast<std::size_t>(length)] + table.count[static_cast<std::size_t>(length)]);
    }
    table.symbol.assign(static_cast<std::size_t>(n), 0);
    for (int symbol = 0; symbol < n; ++symbol) {
      if (lengths[symbol] != 0) {
        table.symbol[offsets[lengths[symbol]]++] = static_cast<std::uint16_t>(symbol);
      }
    }
  }

  int decode(const Huffman& table) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (std::size_t length = 1; length < 16; ++length) {
      code |= bit();
      const int count = table.count[length];
      if (code - count < first) {
        return table.symbol[static_cast<std::size_t>(index + (code - first))];
      }
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
      if (m_overrun) {
        return -1;
      }
    }
    return -1;
  }

  bool stored(std::vector<std::uint8_t>& out) {
    if (m_bit != 0) {
      m_bit = 0;
      ++m_pos;
    }
    if (m_pos + 4 > m_data.size()) {
      return false;
    }
    const std::size_t length = m_data[m_pos] | (static_cast<std::size_t>(m_data[m_pos + 1]) << 8U);
    m_pos += 4;
    if (m_pos + length > m_data.size()) {
      return false;
    }
    out.insert(out.end(), m_data.begin() + static_cast<std::ptrdiff_t>(m_pos),
               m_data.begin() + static_cast<std::ptrdiff_t>(m_pos + length));
    m_pos += length;
    return true;
  }

  bool codes(std::vector<std::uint8_t>& out, const Huffman& literals, const Huffman& distances) {
    static constexpr std::uint16_t kLengthBase[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static constexpr std::uint8_t kLengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static constexpr std::uint16_t kDistanceBase[] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
                                                      33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
                                                      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static constexpr std::uint8_t kDistanceExtra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
      const int symbol = decode(literals);
      if (symbol < 0) {
        return false;
      }
      if (symbol < 256) {
        out.push_back(static_cast<std::uint8_t>(symbol));
        continue;
      }
      if (symbol == 256) {
        return true;
      }
      const int length_code = symbol - 257;
      if (length_code >= 29) {
        return false;
      }
      const std::size_t length = kLengthBase[length_code] + static_cast<std::size_t>(bits(kLengthExtra[length_code]));
      const int distance_code = decode(distances);
      if (distance_code < 0 || distance_code >= 30) {
        return false;
      }
      const std::size_t distance =
          kDistanceBase[distance_code] + static_cast<std::size_t>(bits(kDistanceExtra[distance_code]));
      if (distance > out.size()) {
        return false;
      }
      const std::size_t from = out.size() - distance;
      for (std::size_t index = 0; index < length; ++index) {
        out.push_back(out[from + index]);
      }
    }
  }

  bool fixed(std::vector<std::uint8_t>& out) {
    static const std::pair<Huffman, Huffman> tables = [] {
      std::array<std::uint8_t, 288> lengths{};
      for (std::size_t index = 0; index < 288; ++index) {
        lengths[index] = index < 144 ? 8 : index < 256 ? 9 : index < 280 ? 7 : 8;
      }
      Huffman literals;
      build(literals, lengths.data(), 288);
      std::array<std::uint8_t, 30> distance_lengths{};
      distance_lengths.fill(5);
      Huffman distances;
      build(distances, distance_lengths.data(), 30);
      return std::make_pair(literals, distances);
    }();
    return codes(out, tables.first, tables.second);
  }

  bool dynamic(std::vector<std::uint8_t>& out) {
    static constexpr std::uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int literal_count = bits(5) + 257;
    const int distance_count = bits(5) + 1;
    const int code_count = bits(4) + 4;
    if (literal_count > 286 || distance_count > 30) {
      return false;
    }
    std::array<std::uint8_t, 19> code_lengths{};
    for (int index = 0; index < code_count; ++index) {
      code_lengths[kOrder[index]] = static_cast<std::uint8_t>(bits(3));
    }
    Huffman code_table;
    build(code_table, code_lengths.data(), 19);
    std::array<std::uint8_t, 316> lengths{};
    int index = 0;
    while (index < literal_count + distance_count) {
      const int symbol = decode(code_table);
      if (symbol < 0) {
        return false;
      }
      if (symbol < 16) {
        lengths[static_cast<std::size_t>(index++)] = static_cast<std::uint8_t>(symbol);
        continue;
      }
      std::uint8_t value = 0;
      int repeat = 0;
      if (symbol == 16) {
        if (index == 0) {
          return false;
        }
        value = lengths[static_cast<std::size_t>(index - 1)];
        repeat = 3 + bits(2);
      } else if (symbol == 17) {
        repeat = 3 + bits(3);
      } else {
        repeat = 11 + bits(7);
      }
      if (index + repeat > literal_count + distance_count) {
        return false;
      }
      while (repeat-- > 0) {
        lengths[static_cast<std::size_t>(index++)] = value;
      }
    }
    Huffman literals;
    Huffman distances;
    build(literals, lengths.data(), literal_count);
    build(distances, lengths.data() + literal_count, distance_count);
    return codes(out, literals, distances);
  }

  const std::vector<std::uint8_t>& m_data;
  std::size_t m_pos{0};
  int m_bit{0};
  bool m_overrun{false};
};

[[nodiscard]] std::uint32_t be32(const std::vector<std::uint8_t>& data, const std::size_t at) {
  return (static_cast<std::uint32_t>(data[at]) << 24U) | (static_cast<std::uint32_t>(data[at + 1]) << 16U) |
         (static_cast<std::uint32_t>(data[at + 2]) << 8U) | data[at + 3];
}

[[nodiscard]] std::uint32_t le32(const std::vector<std::uint8_t>& data, const std::size_t at) {
  return data[at] | (static_cast<std::uint32_t>(data[at + 1]) << 8U) | (static_cast<std::uint32_t>(data[at + 2]) << 16U) |
         (static_cast<std::uint32_t>(data[at + 3]) << 24U);
}

bool decode_png(const std::vector<std::uint8_t>& file, Image& image, std::string& error) {
  std::size_t pos = 8;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  int depth = 0;
  int color_type = 0;
  int interlace = 0;
  std::vector<std::uint8_t> compressed;
  std::vector<std::uint32_t> palette;
  while (pos + 8 <= file.size()) {
    const std::uint32_t length = be32(file, pos);
    const std::string type(file.begin() + static_cast<std::ptrdiff_t>(pos + 4), file.begin() + static_cast<std::ptrdiff_t>(pos + 8));
    const std::size_t data = pos + 8;
    if (data + length + 4 > file.size()) {
      error = "truncated PNG";
      return false;
    }
    if (type == "IHDR" && length >= 13) {
      width = be32(file, data);
      height = be32(file, data + 4);
      depth = file[data + 8];
      color_type = file[data + 9];
      interlace = file[data + 12];
    } else if (type == "PLTE") {
      for (std::size_t index = 0; index + 2 < length; index += 3) {
        palette.push_back(0xFF000000U | (static_cast<std::uint32_t>(file[data + index]) << 16U) |
                          (static_cast<std::uint32_t>(file[data + index + 1]) << 8U) | file[data + index + 2]);
      }
    } else if (type == "tRNS" && color_type == 3) {
      for (std::size_t index = 0; index < length && index < palette.size(); ++index) {
        palette[index] = (palette[index] & 0xFFFFFFU) | (static_cast<std::uint32_t>(file[data + index]) << 24U);
      }
    } else if (type == "IDAT") {
      compressed.insert(compressed.end(), file.begin() + static_cast<std::ptrdiff_t>(data),
                        file.begin() + static_cast<std::ptrdiff_t>(data + length));
    } else if (type == "IEND") {
      break;
    }
    pos = data + length + 4;
  }
  if (width == 0 || height == 0 || width > 16384 || height > 16384) {
    error = "invalid PNG size";
    return false;
  }
  if (interlace != 0) {
    error = "interlaced PNG is not supported (save it without interlacing)";
    return false;
  }
  int channels = 0;
  switch (color_type) {
    case 0:
    case 3:
      channels = 1;
      break;
    case 2:
      channels = 3;
      break;
    case 4:
      channels = 2;
      break;
    case 6:
      channels = 4;
      break;
    default:
      error = "unsupported PNG color type";
      return false;
  }
  if (depth != 8 && depth != 16 && !(depth < 8 && (color_type == 0 || color_type == 3))) {
    error = "unsupported PNG bit depth";
    return false;
  }
  std::vector<std::uint8_t> raw;
  Inflater inflater(compressed);
  if (!inflater.run(raw, error)) {
    return false;
  }
  const std::size_t bits_per_pixel = static_cast<std::size_t>(channels * depth);
  const std::size_t stride = (static_cast<std::size_t>(width) * bits_per_pixel + 7) / 8;
  const std::size_t step = std::max<std::size_t>(1, bits_per_pixel / 8);
  if (raw.size() < (stride + 1) * height) {
    error = "truncated PNG data";
    return false;
  }
  std::vector<std::uint8_t> previous(stride, 0);
  std::vector<std::uint8_t> row(stride, 0);
  image.width = static_cast<int>(width);
  image.height = static_cast<int>(height);
  image.pixels.assign(static_cast<std::size_t>(width) * height, 0);
  for (std::size_t y = 0; y < height; ++y) {
    const std::uint8_t filter = raw[y * (stride + 1)];
    const std::uint8_t* line = raw.data() + y * (stride + 1) + 1;
    for (std::size_t x = 0; x < stride; ++x) {
      const int a = x >= step ? row[x - step] : 0;
      const int b = previous[x];
      const int c = x >= step ? previous[x - step] : 0;
      int predicted = 0;
      switch (filter) {
        case 1:
          predicted = a;
          break;
        case 2:
          predicted = b;
          break;
        case 3:
          predicted = (a + b) / 2;
          break;
        case 4: {
          const int p = a + b - c;
          const int pa = std::abs(p - a);
          const int pb = std::abs(p - b);
          const int pc = std::abs(p - c);
          predicted = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
          break;
        }
        default:
          break;
      }
      row[x] = static_cast<std::uint8_t>(line[x] + predicted);
    }
    const auto sample = [&](const std::size_t index) -> std::uint32_t {  // 8-bit value of channel sample `index`
      if (depth == 16) {
        return row[index * 2];
      }
      if (depth == 8) {
        return row[index];
      }
      const std::size_t bit = index * static_cast<std::size_t>(depth);
      const std::uint32_t value =
          (static_cast<std::uint32_t>(row[bit / 8]) >> (8U - static_cast<std::uint32_t>(depth) - bit % 8)) &
          ((1U << static_cast<std::uint32_t>(depth)) - 1U);
      return color_type == 3 ? value : value * 255U / ((1U << static_cast<std::uint32_t>(depth)) - 1U);
    };
    for (std::size_t x = 0; x < width; ++x) {
      std::uint32_t pixel = 0;
      const std::size_t base = x * static_cast<std::size_t>(channels);
      switch (color_type) {
        case 0: {
          const std::uint32_t v = sample(base);
          pixel = 0xFF000000U | (v << 16U) | (v << 8U) | v;
          break;
        }
        case 2:
          pixel = 0xFF000000U | (sample(base) << 16U) | (sample(base + 1) << 8U) | sample(base + 2);
          break;
        case 3: {
          const std::uint32_t index = sample(base);
          pixel = index < palette.size() ? palette[index] : 0xFF000000U;
          break;
        }
        case 4: {
          const std::uint32_t v = sample(base);
          pixel = (sample(base + 1) << 24U) | (v << 16U) | (v << 8U) | v;
          break;
        }
        default:
          pixel = (sample(base + 3) << 24U) | (sample(base) << 16U) | (sample(base + 1) << 8U) | sample(base + 2);
          break;
      }
      image.pixels[y * width + x] = pixel;
    }
    std::swap(previous, row);
  }
  return true;
}

bool decode_bmp(const std::vector<std::uint8_t>& file, Image& image, std::string& error) {
  if (file.size() < 54) {
    error = "truncated BMP";
    return false;
  }
  const std::uint32_t offset = le32(file, 10);
  const auto width = static_cast<std::int32_t>(le32(file, 18));
  const auto raw_height = static_cast<std::int32_t>(le32(file, 22));
  const int bpp = file[28] | (file[29] << 8);
  const std::uint32_t compression = le32(file, 30);
  if ((bpp != 24 && bpp != 32) || (compression != 0 && compression != 3) || width <= 0 || raw_height == 0 ||
      width > 16384 || std::abs(raw_height) > 16384) {
    error = "only uncompressed 24/32-bit BMP is supported";
    return false;
  }
  const bool top_down = raw_height < 0;
  const int height = std::abs(raw_height);
  const std::size_t bytes = static_cast<std::size_t>(bpp / 8);
  const std::size_t stride = (static_cast<std::size_t>(width) * bytes + 3) & ~static_cast<std::size_t>(3);
  if (offset + stride * static_cast<std::size_t>(height) > file.size()) {
    error = "truncated BMP";
    return false;
  }
  image.width = width;
  image.height = height;
  image.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
  bool any_alpha = false;
  for (int y = 0; y < height; ++y) {
    const std::size_t source_row = static_cast<std::size_t>(top_down ? y : height - 1 - y);
    for (int x = 0; x < width; ++x) {
      const std::size_t at = offset + source_row * stride + static_cast<std::size_t>(x) * bytes;
      const std::uint32_t alpha = bpp == 32 ? file[at + 3] : 255U;
      any_alpha = any_alpha || (bpp == 32 && alpha != 0);
      image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
          (alpha << 24U) | (static_cast<std::uint32_t>(file[at + 2]) << 16U) | (static_cast<std::uint32_t>(file[at + 1]) << 8U) |
          file[at];
    }
  }
  if (bpp == 32 && !any_alpha) {  // many tools write 32-bit BMPs with an unused alpha byte
    for (std::uint32_t& pixel : image.pixels) {
      pixel |= 0xFF000000U;
    }
  }
  return true;
}

[[nodiscard]] bool unsafe_path(const std::string& path) { return path.find("..") != std::string::npos; }

// --- saving -------------------------------------------------------------------------------------

[[nodiscard]] std::uint32_t crc32(const std::uint8_t* data, const std::size_t size, std::uint32_t crc = 0xFFFFFFFFU) {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> entries{};
    for (std::uint32_t index = 0; index < 256; ++index) {
      std::uint32_t value = index;
      for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1U) != 0 ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
      }
      entries[index] = value;
    }
    return entries;
  }();
  for (std::size_t index = 0; index < size; ++index) {
    crc = table[(crc ^ data[index]) & 0xFFU] ^ (crc >> 8U);
  }
  return crc;
}

void put32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24U));
  out.push_back(static_cast<std::uint8_t>(value >> 16U));
  out.push_back(static_cast<std::uint8_t>(value >> 8U));
  out.push_back(static_cast<std::uint8_t>(value));
}

void chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
  put32(out, static_cast<std::uint32_t>(data.size()));
  const std::size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  put32(out, crc32(out.data() + start, out.size() - start) ^ 0xFFFFFFFFU);
}

// PNG with "stored" (uncompressed) deflate blocks: bigger than a compressed PNG, but simple,
// exact and readable by every viewer.
[[nodiscard]] std::vector<std::uint8_t> encode_png(const int width, const int height, const std::vector<std::uint32_t>& pixels) {
  std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<std::uint8_t> header;
  put32(header, static_cast<std::uint32_t>(width));
  put32(header, static_cast<std::uint32_t>(height));
  header.insert(header.end(), {8, 2, 0, 0, 0});  // 8-bit RGB
  chunk(out, "IHDR", header);
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * 3 + 1));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    for (int x = 0; x < width; ++x) {
      const std::uint32_t pixel = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
      raw.push_back(static_cast<std::uint8_t>(pixel >> 16U));
      raw.push_back(static_cast<std::uint8_t>(pixel >> 8U));
      raw.push_back(static_cast<std::uint8_t>(pixel));
    }
  }
  std::vector<std::uint8_t> zlib = {0x78, 0x01};
  std::size_t offset = 0;
  do {
    const std::size_t block = std::min<std::size_t>(65535, raw.size() - offset);
    zlib.push_back(offset + block == raw.size() ? 1 : 0);
    zlib.push_back(static_cast<std::uint8_t>(block));
    zlib.push_back(static_cast<std::uint8_t>(block >> 8U));
    zlib.push_back(static_cast<std::uint8_t>(~block));
    zlib.push_back(static_cast<std::uint8_t>(~block >> 8U));
    zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                raw.begin() + static_cast<std::ptrdiff_t>(offset + block));
    offset += block;
  } while (offset < raw.size());
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (const std::uint8_t byte : raw) {
    a = (a + byte) % 65521U;
    b = (b + a) % 65521U;
  }
  put32(zlib, (b << 16U) | a);
  chunk(out, "IDAT", zlib);
  chunk(out, "IEND", {});
  return out;
}

[[nodiscard]] std::vector<std::uint8_t> encode_bmp(const int width, const int height, const std::vector<std::uint32_t>& pixels) {
  const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~static_cast<std::size_t>(3);
  const std::size_t size = 54 + stride * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> out(size, 0);
  const auto le = [&](const std::size_t at, const std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
      out[at + index] = static_cast<std::uint8_t>(value >> (8U * index));
    }
  };
  out[0] = 'B';
  out[1] = 'M';
  le(2, static_cast<std::uint32_t>(size));
  le(10, 54);
  le(14, 40);
  le(18, static_cast<std::uint32_t>(width));
  le(22, static_cast<std::uint32_t>(height));
  out[26] = 1;
  out[28] = 24;
  le(34, static_cast<std::uint32_t>(stride * static_cast<std::size_t>(height)));
  for (int y = 0; y < height; ++y) {
    const std::size_t row = 54 + static_cast<std::size_t>(height - 1 - y) * stride;
    for (int x = 0; x < width; ++x) {
      const std::uint32_t pixel = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
      out[row + static_cast<std::size_t>(x) * 3] = static_cast<std::uint8_t>(pixel);
      out[row + static_cast<std::size_t>(x) * 3 + 1] = static_cast<std::uint8_t>(pixel >> 8U);
      out[row + static_cast<std::size_t>(x) * 3 + 2] = static_cast<std::uint8_t>(pixel >> 16U);
    }
  }
  return out;
}

[[nodiscard]] bool ends_with(const std::string& text, const std::string_view suffix) {
  if (text.size() < suffix.size()) {
    return false;
  }
  for (std::size_t index = 0; index < suffix.size(); ++index) {
    const auto c = static_cast<unsigned char>(text[text.size() - suffix.size() + index]);
    if (static_cast<char>(std::tolower(c)) != suffix[index]) {
      return false;
    }
  }
  return true;
}

// --- post-processing ("shaders" on the CPU) ---------------------------------------------------

template <typename Visit>
void each_pixel(Visit&& visit) {
  const Clip area = bounds();
  for (int y = area.y0; y < area.y1; ++y) {
    std::uint32_t* row = g_canvas.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width);
    for (int x = area.x0; x < area.x1; ++x) {
      visit(row[x]);
    }
  }
}

struct Rgb {
  double r;
  double g;
  double b;
};

[[nodiscard]] Rgb unpack(const std::uint32_t pixel) {
  return Rgb{static_cast<double>((pixel >> 16U) & 0xFFU), static_cast<double>((pixel >> 8U) & 0xFFU),
             static_cast<double>(pixel & 0xFFU)};
}

[[nodiscard]] std::uint32_t pack(const Rgb& color) {
  const auto channel = [](const double value) {
    return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0, 255.0)));
  };
  return 0xFF000000U | (channel(color.r) << 16U) | (channel(color.g) << 8U) | channel(color.b);
}

[[nodiscard]] double luma(const Rgb& color) { return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b; }

// Box blur of the visible area, `passes` times (3 passes approximate a Gaussian).
void box_blur(std::vector<Rgb>& image, const int width, const int height, const int radius, const int passes) {
  if (radius <= 0 || width <= 0 || height <= 0) {
    return;
  }
  std::vector<Rgb> temp(image.size());
  const double scale = 1.0 / (2 * radius + 1);
  for (int pass = 0; pass < passes; ++pass) {
    for (int y = 0; y < height; ++y) {  // horizontal
      Rgb sum{0, 0, 0};
      const auto at = [&](int x) -> const Rgb& {
        x = std::clamp(x, 0, width - 1);
        return image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
      };
      for (int x = -radius; x <= radius; ++x) {
        sum.r += at(x).r, sum.g += at(x).g, sum.b += at(x).b;
      }
      for (int x = 0; x < width; ++x) {
        temp[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
            Rgb{sum.r * scale, sum.g * scale, sum.b * scale};
        const Rgb& add = at(x + radius + 1);
        const Rgb& remove = at(x - radius);
        sum.r += add.r - remove.r, sum.g += add.g - remove.g, sum.b += add.b - remove.b;
      }
    }
    for (int x = 0; x < width; ++x) {  // vertical
      Rgb sum{0, 0, 0};
      const auto at = [&](int y) -> const Rgb& {
        y = std::clamp(y, 0, height - 1);
        return temp[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
      };
      for (int y = -radius; y <= radius; ++y) {
        sum.r += at(y).r, sum.g += at(y).g, sum.b += at(y).b;
      }
      for (int y = 0; y < height; ++y) {
        image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
            Rgb{sum.r * scale, sum.g * scale, sum.b * scale};
        const Rgb& add = at(y + radius + 1);
        const Rgb& remove = at(y - radius);
        sum.r += add.r - remove.r, sum.g += add.g - remove.g, sum.b += add.b - remove.b;
      }
    }
  }
}

bool apply_effect(const std::string& raw_name, const double amount, std::string& error) {
  std::string name;
  for (const char c : raw_name) {
    name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  const Clip area = bounds();
  const int width = area.x1 - area.x0;
  const int height = area.y1 - area.y0;
  if (width <= 0 || height <= 0) {
    return true;
  }
  const auto read_area = [&] {
    std::vector<Rgb> image(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
            unpack(g_canvas.pixels[static_cast<std::size_t>(y + area.y0) * static_cast<std::size_t>(g_canvas.width) +
                                   static_cast<std::size_t>(x + area.x0)]);
      }
    }
    return image;
  };
  const auto write_area = [&](const std::vector<Rgb>& image) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        g_canvas.pixels[static_cast<std::size_t>(y + area.y0) * static_cast<std::size_t>(g_canvas.width) +
                        static_cast<std::size_t>(x + area.x0)] =
            pack(image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]);
      }
    }
  };
  const double t = saturate(amount);
  const auto per_pixel = [&](auto&& change) {
    each_pixel([&](std::uint32_t& pixel) {
      const Rgb before = unpack(pixel);
      const Rgb after = change(before);
      pixel = pack(after);
    });
  };
  if (name == "grayscale" || name == "greyscale") {
    per_pixel([&](const Rgb& c) {
      const double l = luma(c);
      return Rgb{c.r + (l - c.r) * t, c.g + (l - c.g) * t, c.b + (l - c.b) * t};
    });
  } else if (name == "sepia") {
    per_pixel([&](const Rgb& c) {
      const Rgb s{c.r * 0.393 + c.g * 0.769 + c.b * 0.189, c.r * 0.349 + c.g * 0.686 + c.b * 0.168,
                  c.r * 0.272 + c.g * 0.534 + c.b * 0.131};
      return Rgb{c.r + (s.r - c.r) * t, c.g + (s.g - c.g) * t, c.b + (s.b - c.b) * t};
    });
  } else if (name == "invert") {
    per_pixel([&](const Rgb& c) {
      return Rgb{c.r + (255 - 2 * c.r) * t, c.g + (255 - 2 * c.g) * t, c.b + (255 - 2 * c.b) * t};
    });
  } else if (name == "brightness") {
    const double shift = std::clamp(amount, -1.0, 1.0) * 255.0;
    per_pixel([&](const Rgb& c) { return Rgb{c.r + shift, c.g + shift, c.b + shift}; });
  } else if (name == "contrast") {
    const double factor = std::max(0.0, amount);
    per_pixel([&](const Rgb& c) {
      return Rgb{(c.r - 128) * factor + 128, (c.g - 128) * factor + 128, (c.b - 128) * factor + 128};
    });
  } else if (name == "saturation") {
    const double factor = std::max(0.0, amount);
    per_pixel([&](const Rgb& c) {
      const double l = luma(c);
      return Rgb{l + (c.r - l) * factor, l + (c.g - l) * factor, l + (c.b - l) * factor};
    });
  } else if (name == "threshold") {
    per_pixel([&](const Rgb& c) {
      const double v = luma(c) >= t * 255 ? 255 : 0;
      return Rgb{v, v, v};
    });
  } else if (name == "posterize") {
    const double levels = std::max(2.0, std::round(amount));
    per_pixel([&](const Rgb& c) {
      const auto step = [&](const double v) { return std::round(v / 255.0 * (levels - 1)) / (levels - 1) * 255.0; };
      return Rgb{step(c.r), step(c.g), step(c.b)};
    });
  } else if (name == "vignette") {
    const double cx = area.x0 + width / 2.0;
    const double cy = area.y0 + height / 2.0;
    const double reach = std::sqrt(cx * cx + cy * cy) > 0 ? std::hypot(width / 2.0, height / 2.0) : 1;
    for (int y = area.y0; y < area.y1; ++y) {
      for (int x = area.x0; x < area.x1; ++x) {
        std::uint32_t& pixel = g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                                               static_cast<std::size_t>(x)];
        const double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy) / reach;
        const double dark = 1.0 - t * std::pow(saturate((d - 0.35) / 0.65), 1.6);
        const Rgb c = unpack(pixel);
        pixel = pack(Rgb{c.r * dark, c.g * dark, c.b * dark});
      }
    }
  } else if (name == "scanlines") {
    for (int y = area.y0; y < area.y1; y += 2) {
      for (int x = area.x0; x < area.x1; ++x) {
        std::uint32_t& pixel = g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                                               static_cast<std::size_t>(x)];
        const Rgb c = unpack(pixel);
        pixel = pack(Rgb{c.r * (1 - t * 0.6), c.g * (1 - t * 0.6), c.b * (1 - t * 0.6)});
      }
    }
  } else if (name == "noise" || name == "grain") {
    static std::uint32_t seed = 0x9E3779B9U;
    per_pixel([&](const Rgb& c) {
      seed ^= seed << 13U;
      seed ^= seed >> 17U;
      seed ^= seed << 5U;
      const double n = (static_cast<double>(seed & 0xFFFFU) / 65535.0 - 0.5) * 2.0 * t * 64.0;
      return Rgb{c.r + n, c.g + n, c.b + n};
    });
  } else if (name == "pixelate") {
    const int block = std::max(1, static_cast<int>(std::round(amount)));
    for (int by = area.y0; by < area.y1; by += block) {
      for (int bx = area.x0; bx < area.x1; bx += block) {
        Rgb sum{0, 0, 0};
        int count = 0;
        for (int y = by; y < std::min(by + block, area.y1); ++y) {
          for (int x = bx; x < std::min(bx + block, area.x1); ++x) {
            const Rgb c = unpack(g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                                                 static_cast<std::size_t>(x)]);
            sum.r += c.r, sum.g += c.g, sum.b += c.b;
            ++count;
          }
        }
        const std::uint32_t average = pack(Rgb{sum.r / count, sum.g / count, sum.b / count});
        for (int y = by; y < std::min(by + block, area.y1); ++y) {
          for (int x = bx; x < std::min(bx + block, area.x1); ++x) {
            g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) + static_cast<std::size_t>(x)] =
                average;
          }
        }
      }
    }
  } else if (name == "blur") {
    std::vector<Rgb> image = read_area();
    box_blur(image, width, height, std::max(0, static_cast<int>(std::round(amount / 2))), 3);
    write_area(image);
  } else if (name == "sharpen") {
    std::vector<Rgb> image = read_area();
    std::vector<Rgb> soft = image;
    box_blur(soft, width, height, 1, 1);
    for (std::size_t index = 0; index < image.size(); ++index) {
      image[index].r += (image[index].r - soft[index].r) * amount;
      image[index].g += (image[index].g - soft[index].g) * amount;
      image[index].b += (image[index].b - soft[index].b) * amount;
    }
    write_area(image);
  } else if (name == "bloom" || name == "glow") {
    std::vector<Rgb> image = read_area();
    std::vector<Rgb> bright(image.size());
    for (std::size_t index = 0; index < image.size(); ++index) {
      const double over = std::max(0.0, luma(image[index]) - 150.0) / 105.0;
      bright[index] = Rgb{image[index].r * over, image[index].g * over, image[index].b * over};
    }
    box_blur(bright, width, height, std::max(2, std::min(width, height) / 60), 3);
    for (std::size_t index = 0; index < image.size(); ++index) {
      image[index].r += bright[index].r * amount;
      image[index].g += bright[index].g * amount;
      image[index].b += bright[index].b * amount;
    }
    write_area(image);
  } else if (name == "chromatic") {
    const std::vector<Rgb> image = read_area();
    const int shift = static_cast<int>(std::round(amount));
    std::vector<Rgb> result = image;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto at = [&](const int sx) -> const Rgb& {
          return image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(std::clamp(sx, 0, width - 1))];
        };
        Rgb& target = result[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
        target.r = at(x - shift).r;
        target.b = at(x + shift).b;
      }
    }
    write_area(result);
  } else {
    return fail(error, "Gfx.Effect: unknown effect \"" + raw_name +
                           "\" (grayscale, sepia, invert, brightness, contrast, saturation, threshold, posterize, "
                           "vignette, scanlines, noise, pixelate, blur, sharpen, bloom, chromatic)");
  }
  return true;
}

// --- natives ------------------------------------------------------------------------------------

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(gfx_canvas) {
  if (!numbers(args, arity, error, "Gfx.Canvas")) {
    return false;
  }
  const int width = static_cast<int>(args[0].number);
  const int height = static_cast<int>(args[1].number);
  if (width < 1 || height < 1 || width > 8192 || height > 8192) {
    return fail(error, "Gfx.Canvas: size must be between 1 and 8192");
  }
  resize_canvas(width, height);
  (void)out;
  return true;
}

CLPP_NATIVE(gfx_width) {
  (void)args;
  (void)arity;
  (void)error;
  out = Value::number_of(g_canvas.width);
  return true;
}

CLPP_NATIVE(gfx_height) {
  (void)args;
  (void)arity;
  (void)error;
  out = Value::number_of(g_canvas.height);
  return true;
}

CLPP_NATIVE(gfx_clear) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Clear") || !require_canvas(error)) {
    return false;
  }
  const Paint paint = paint_of(color_bits(args[0].number));
  if (paint.alpha >= 0.998 && g_clip.x0 <= 0 && g_clip.y0 <= 0 && g_clip.x1 >= g_canvas.width && g_clip.y1 >= g_canvas.height) {
    std::fill(g_canvas.pixels.begin(), g_canvas.pixels.end(), 0xFF000000U | paint.rgb);
    return true;
  }
  fill_rect(0, 0, g_canvas.width, g_canvas.height, color_bits(args[0].number));
  return true;
}

CLPP_NATIVE(gfx_pixel) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Pixel") || !require_canvas(error)) {
    return false;
  }
  plot(static_cast<int>(std::floor(args[0].number)), static_cast<int>(std::floor(args[1].number)),
       paint_of(color_bits(args[2].number)), 1.0, bounds());
  return true;
}

CLPP_NATIVE(gfx_get_pixel) {
  if (!numbers(args, arity, error, "Gfx.GetPixel") || !require_canvas(error)) {
    return false;
  }
  const int x = static_cast<int>(std::floor(args[0].number));
  const int y = static_cast<int>(std::floor(args[1].number));
  if (x < 0 || y < 0 || x >= g_canvas.width || y >= g_canvas.height) {
    out = Value::number_of(0);
    return true;
  }
  out = Value::number_of(g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                                         static_cast<std::size_t>(x)] &
                         0xFFFFFFU);
  return true;
}

CLPP_NATIVE(gfx_rect) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Rect") || !require_canvas(error)) {
    return false;
  }
  fill_rect(args[0].number, args[1].number, args[2].number, args[3].number, color_bits(args[4].number));
  return true;
}

CLPP_NATIVE(gfx_rect_line) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.RectLine") || !require_canvas(error)) {
    return false;
  }
  const double thickness = arity == 6 ? args[4].number : 1.0;
  stroke_rect(args[0].number, args[1].number, args[2].number, args[3].number, thickness,
              color_bits(args[arity - 1].number));
  return true;
}

CLPP_NATIVE(gfx_round_rect) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.RoundRect") || !require_canvas(error)) {
    return false;
  }
  fill_round_rect(args[0].number, args[1].number, args[2].number, args[3].number, args[4].number,
                  color_bits(args[5].number));
  return true;
}

CLPP_NATIVE(gfx_circle) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Circle") || !require_canvas(error)) {
    return false;
  }
  fill_circle(args[0].number, args[1].number, args[2].number, color_bits(args[3].number));
  return true;
}

CLPP_NATIVE(gfx_circle_line) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.CircleLine") || !require_canvas(error)) {
    return false;
  }
  const double thickness = arity == 5 ? args[3].number : 1.0;
  draw_ring(args[0].number, args[1].number, args[2].number, thickness, color_bits(args[arity - 1].number));
  return true;
}

CLPP_NATIVE(gfx_line) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Line") || !require_canvas(error)) {
    return false;
  }
  const double thickness = arity == 6 ? args[4].number : 1.0;
  draw_line(args[0].number, args[1].number, args[2].number, args[3].number, thickness, color_bits(args[arity - 1].number));
  return true;
}

CLPP_NATIVE(gfx_triangle) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Triangle") || !require_canvas(error)) {
    return false;
  }
  fill_triangle(args[0].number, args[1].number, args[2].number, args[3].number, args[4].number, args[5].number,
                color_bits(args[6].number));
  return true;
}

CLPP_NATIVE(gfx_gradient) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Gradient") || !require_canvas(error)) {
    return false;
  }
  fill_gradient(args[0].number, args[1].number, args[2].number, args[3].number, color_bits(args[4].number),
                color_bits(args[5].number), arity == 7 && args[6].number != 0);
  return true;
}

CLPP_NATIVE(gfx_text) {
  (void)out;
  if (arity != 5 || !args[0].is_string() || !numbers(args + 1, 4, error, "Gfx.Text")) {
    return error.empty() ? fail(error, "Gfx.Text: expected (string text, x, y, size, color)") : false;
  }
  if (!require_canvas(error)) {
    return false;
  }
  draw_text(args[0].text, args[1].number, args[2].number, args[3].number, color_bits(args[4].number));
  return true;
}

CLPP_NATIVE(gfx_text_width) {
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Gfx.TextWidth: expected (string text, float size)");
  }
  out = Value::number_of(text_width(args[0].text, args[1].number));
  return true;
}

CLPP_NATIVE(gfx_text_height) {
  if (!numbers(args, arity, error, "Gfx.TextHeight")) {
    return false;
  }
  out = Value::number_of(text_height(args[0].number));
  return true;
}

CLPP_NATIVE(gfx_font) {  // (family): "pixel" = the built-in bitmap font
  (void)out;
  if (arity != 1 || !args[0].is_string() || args[0].text.empty()) {
    return fail(error, "Gfx.Font: expected a font family such as \"Segoe UI\", or \"pixel\"");
  }
  text::FontChoice& font = text::choice();
  font.pixel = args[0].text == "pixel";
  if (!font.pixel) {
    font.family = args[0].text;
  }
  return true;
}

CLPP_NATIVE(gfx_bold) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Bold")) {
    return false;
  }
  text::choice().bold = args[0].number != 0;
  return true;
}

CLPP_NATIVE(gfx_shadow) {  // (x, y, w, h, radius, blur, color)
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Shadow") || !require_canvas(error)) {
    return false;
  }
  fill_shadow(args[0].number, args[1].number, args[2].number, args[3].number, args[4].number, args[5].number,
              color_bits(args[6].number));
  return true;
}

CLPP_NATIVE(gfx_round_rect_line) {  // (x, y, w, h, radius, thickness, color)
  (void)out;
  if (!numbers(args, arity, error, "Gfx.RoundRectLine") || !require_canvas(error)) {
    return false;
  }
  stroke_round_rect(args[0].number, args[1].number, args[2].number, args[3].number, args[4].number, args[5].number,
                    color_bits(args[6].number));
  return true;
}

CLPP_NATIVE(gfx_effect) {  // (name, amount)
  (void)out;
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Gfx.Effect: expected (string name, float amount)");
  }
  if (!require_canvas(error)) {
    return false;
  }
  return apply_effect(args[0].text, args[1].number, error);
}

CLPP_NATIVE(gfx_tint) {  // (color, amount)
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Tint") || !require_canvas(error)) {
    return false;
  }
  const std::uint32_t rgb = color_bits(args[0].number) & 0xFFFFFFU;
  const double amount = saturate(args[1].number);
  each_pixel([&](std::uint32_t& pixel) { blend(pixel, rgb, amount); });
  return true;
}

CLPP_NATIVE(gfx_clip) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Clip")) {
    return false;
  }
  g_clip = Clip{static_cast<int>(std::floor(args[0].number)), static_cast<int>(std::floor(args[1].number)),
                static_cast<int>(std::ceil(args[0].number + args[2].number)),
                static_cast<int>(std::ceil(args[1].number + args[3].number))};
  return true;
}

CLPP_NATIVE(gfx_no_clip) {
  (void)args;
  (void)arity;
  (void)out;
  (void)error;
  g_clip = Clip{};
  return true;
}

CLPP_NATIVE(gfx_rgb) {
  if (!numbers(args, arity, error, "Gfx.Rgb")) {
    return false;
  }
  std::uint32_t color = 0;
  for (std::uint8_t index = 0; index < 3; ++index) {
    color = (color << 8U) | static_cast<std::uint32_t>(clamp_int(std::round(args[index].number), 0, 255));
  }
  if (arity == 4) {
    color |= static_cast<std::uint32_t>(255 - clamp_int(std::round(args[3].number), 0, 255)) << 24U;
  }
  out = Value::number_of(color);
  return true;
}

CLPP_NATIVE(gfx_hsv) {
  if (!numbers(args, arity, error, "Gfx.Hsv")) {
    return false;
  }
  double h = std::fmod(args[0].number, 360.0);
  if (h < 0) {
    h += 360.0;
  }
  const double s = saturate(args[1].number);
  const double v = saturate(args[2].number);
  const double c = v * s;
  const double x = c * (1 - std::fabs(std::fmod(h / 60.0, 2.0) - 1));
  const double m = v - c;
  double r = 0;
  double g = 0;
  double b = 0;
  if (h < 60) {
    r = c, g = x;
  } else if (h < 120) {
    r = x, g = c;
  } else if (h < 180) {
    g = c, b = x;
  } else if (h < 240) {
    g = x, b = c;
  } else if (h < 300) {
    r = x, b = c;
  } else {
    r = c, b = x;
  }
  const auto channel = [&](const double value) { return static_cast<std::uint32_t>(std::lround((value + m) * 255.0)); };
  out = Value::number_of((channel(r) << 16U) | (channel(g) << 8U) | channel(b));
  return true;
}

CLPP_NATIVE(gfx_mix) {
  if (!numbers(args, arity, error, "Gfx.Mix")) {
    return false;
  }
  out = Value::number_of(mix_color(color_bits(args[0].number), color_bits(args[1].number), saturate(args[2].number)));
  return true;
}

CLPP_NATIVE(gfx_fade) {
  if (!numbers(args, arity, error, "Gfx.Fade")) {
    return false;
  }
  const std::uint32_t color = color_bits(args[0].number);
  const double alpha = paint_of(color).alpha * saturate(args[1].number);
  const auto transparency = static_cast<std::uint32_t>(std::lround((1.0 - alpha) * 255.0));
  out = Value::number_of((color & 0xFFFFFFU) | (transparency << 24U));
  return true;
}

CLPP_NATIVE(gfx_hex) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Gfx.Hex: expected a string like \"#FF8800\"");
  }
  std::string text = args[0].text;
  if (!text.empty() && text.front() == '#') {
    text.erase(0, 1);
  }
  if (text.size() == 3) {
    text = {text[0], text[0], text[1], text[1], text[2], text[2]};
  }
  if (text.size() != 6 || text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
    return fail(error, "Gfx.Hex: invalid color \"" + args[0].text + "\"");
  }
  out = Value::number_of(static_cast<double>(std::stoul(text, nullptr, 16)));
  return true;
}

CLPP_NATIVE(gfx_channel) {  // Red/Green/Blue share one native: (color, shift)
  if (!numbers(args, arity, error, "Gfx color channel")) {
    return false;
  }
  out = Value::number_of((color_bits(args[0].number) >> static_cast<std::uint32_t>(args[1].number)) & 0xFFU);
  return true;
}

[[nodiscard]] int load_image(const std::string& path, std::string& error) {
  if (unsafe_path(path)) {
    error = "Gfx.LoadImage: paths with '..' are refused";
    return 0;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open image: " + path;
    return 0;
  }
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  Image image;
  std::string reason;
  bool ok = false;
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    ok = decode_png(bytes, image, reason);
  } else if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
    ok = decode_bmp(bytes, image, reason);
  } else {
    reason = "not a PNG or BMP file";
  }
  if (!ok) {
    error = "cannot load image " + path + ": " + reason;
    return 0;
  }
  g_images.push_back(std::move(image));
  return static_cast<int>(g_images.size());
}

CLPP_NATIVE(gfx_load_image) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Gfx.LoadImage: expected a file path");
  }
  const int handle = load_image(args[0].text, error);
  if (handle == 0) {
    return false;
  }
  out = Value::number_of(handle);
  return true;
}

CLPP_NATIVE(gfx_image_size) {  // (image, 0 = width | 1 = height)
  if (!numbers(args, arity, error, "Gfx.ImageWidth")) {
    return false;
  }
  const Image* image = image_at(args[0].number, error);
  if (image == nullptr) {
    return false;
  }
  out = Value::number_of(args[1].number == 0 ? image->width : image->height);
  return true;
}

CLPP_NATIVE(gfx_image) {
  (void)out;
  if (!numbers(args, arity, error, "Gfx.Image") || !require_canvas(error)) {
    return false;
  }
  const Image* image = image_at(args[0].number, error);
  if (image == nullptr) {
    return false;
  }
  const double w = arity == 5 ? args[3].number : image->width;
  const double h = arity == 5 ? args[4].number : image->height;
  draw_image(*image, 0, 0, image->width, image->height, args[1].number, args[2].number, w, h);
  return true;
}

CLPP_NATIVE(gfx_image_part) {  // (image, sx, sy, sw, sh, x, y, scale)
  (void)out;
  if (!numbers(args, arity, error, "Gfx.ImagePart") || !require_canvas(error)) {
    return false;
  }
  const Image* image = image_at(args[0].number, error);
  if (image == nullptr) {
    return false;
  }
  const double scale = args[7].number;
  draw_image(*image, args[1].number, args[2].number, args[3].number, args[4].number, args[5].number, args[6].number,
             args[3].number * scale, args[4].number * scale);
  return true;
}

CLPP_NATIVE(gfx_capture) {
  if (!numbers(args, arity, error, "Gfx.Capture") || !require_canvas(error)) {
    return false;
  }
  const int x0 = clamp_int(std::round(args[0].number), 0, g_canvas.width);
  const int y0 = clamp_int(std::round(args[1].number), 0, g_canvas.height);
  const int x1 = clamp_int(std::round(args[0].number + args[2].number), 0, g_canvas.width);
  const int y1 = clamp_int(std::round(args[1].number + args[3].number), 0, g_canvas.height);
  if (x1 <= x0 || y1 <= y0) {
    return fail(error, "Gfx.Capture: empty area");
  }
  Image image;
  image.width = x1 - x0;
  image.height = y1 - y0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      image.pixels.push_back(g_canvas.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(g_canvas.width) +
                                             static_cast<std::size_t>(x)]);
    }
  }
  g_images.push_back(std::move(image));
  out = Value::number_of(static_cast<double>(g_images.size()));
  return true;
}

CLPP_NATIVE(gfx_save) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Gfx.Save: expected a file path ending in .png or .bmp");
  }
  if (!require_canvas(error)) {
    return false;
  }
  const std::string& path = args[0].text;
  if (unsafe_path(path)) {
    return fail(error, "Gfx.Save: paths with '..' are refused");
  }
  std::vector<std::uint8_t> bytes;
  if (ends_with(path, ".png")) {
    bytes = encode_png(g_canvas.width, g_canvas.height, g_canvas.pixels);
  } else if (ends_with(path, ".bmp")) {
    bytes = encode_bmp(g_canvas.width, g_canvas.height, g_canvas.pixels);
  } else {
    return fail(error, "Gfx.Save: the file name must end in .png or .bmp");
  }
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  out = boolean(static_cast<bool>(file));
  return true;
}

CLPP_NATIVE(gfx_checksum) {
  (void)args;
  (void)arity;
  (void)error;
  std::uint32_t hash = 2166136261U;
  for (const std::uint32_t pixel : g_canvas.pixels) {
    for (std::uint32_t shift = 0; shift < 24; shift += 8) {
      hash = (hash ^ ((pixel >> shift) & 0xFFU)) * 16777619U;
    }
  }
  out = Value::number_of(hash);
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kGfxSource = R"clp(<< @clpp.gfx: 2D drawing on a software canvas (shown by @clpp.window, saved by Save).
<< Colors are ints 0xRRGGBB; the top byte is transparency (0 = opaque). See Fade and Rgba.
<< Text sizes are pixels (like CSS font-size); Font("pixel") switches to the built-in pixel font.

const BLACK = 0x000000;
const WHITE = 0xFFFFFF;
const GRAY = 0x9E9E9E;
const DARK = 0x1E1E2E;
const LIGHT = 0xF4F4F8;
const RED = 0xE5484D;
const ORANGE = 0xF76B15;
const YELLOW = 0xFFC53D;
const GREEN = 0x30A46C;
const TEAL = 0x12A594;
const CYAN = 0x00A2C7;
const BLUE = 0x3E63DD;
const PURPLE = 0x8E4EC6;
const PINK = 0xD6409F;
const BROWN = 0xAD7F58;
const TRANSPARENT = 0xFF000000;

func Canvas(int width, int height) { gfx::Canvas(width, height); }
func Width() -> int { return gfx::Width(); }
func Height() -> int { return gfx::Height(); }
func Clear(int color) { gfx::Clear(color); }
func Pixel(float x, float y, int color) { gfx::Pixel(x, y, color); }
func GetPixel(float x, float y) -> int { return gfx::GetPixel(x, y); }
func Rect(float x, float y, float width, float height, int color) { gfx::Rect(x, y, width, height, color); }
func RectLine(float x, float y, float width, float height, int color) { gfx::RectLine(x, y, width, height, color); }
func RectLine(float x, float y, float width, float height, float thickness, int color) { gfx::RectLine(x, y, width, height, thickness, color); }
func RoundRect(float x, float y, float width, float height, float radius, int color) { gfx::RoundRect(x, y, width, height, radius, color); }
func Circle(float x, float y, float radius, int color) { gfx::Circle(x, y, radius, color); }
func CircleLine(float x, float y, float radius, int color) { gfx::CircleLine(x, y, radius, color); }
func CircleLine(float x, float y, float radius, float thickness, int color) { gfx::CircleLine(x, y, radius, thickness, color); }
func Line(float x1, float y1, float x2, float y2, int color) { gfx::Line(x1, y1, x2, y2, color); }
func Line(float x1, float y1, float x2, float y2, float thickness, int color) { gfx::Line(x1, y1, x2, y2, thickness, color); }
func Triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color) { gfx::Triangle(x1, y1, x2, y2, x3, y3, color); }
func Gradient(float x, float y, float width, float height, int top, int bottom) { gfx::Gradient(x, y, width, height, top, bottom); }
func GradientH(float x, float y, float width, float height, int left, int right) { gfx::Gradient(x, y, width, height, left, right, 1); }
func Text(string text, float x, float y, float size, int color) { gfx::Text(text, x, y, size, color); }
func TextWidth(string text, float size) -> float { return gfx::TextWidth(text, size); }
func TextHeight(float size) -> float { return gfx::TextHeight(size); }
func Font(string family) { gfx::Font(family); }
func Bold(bool bold) { gfx::Bold(bold); }
func Shadow(float x, float y, float width, float height, float radius, float blur, int color) { gfx::Shadow(x, y, width, height, radius, blur, color); }
func RoundRectLine(float x, float y, float width, float height, float radius, float thickness, int color) { gfx::RoundRectLine(x, y, width, height, radius, thickness, color); }
func Effect(string name, float amount) { gfx::Effect(name, amount); }
func Tint(int color, float amount) { gfx::Tint(color, amount); }
func Clip(float x, float y, float width, float height) { gfx::Clip(x, y, width, height); }
func NoClip() { gfx::NoClip(); }
func Rgb(int r, int g, int b) -> int { return gfx::Rgb(r, g, b); }
func Rgba(int r, int g, int b, int a) -> int { return gfx::Rgb(r, g, b, a); }
func Hsv(float hue, float saturation, float value) -> int { return gfx::Hsv(hue, saturation, value); }
func Mix(int a, int b, float t) -> int { return gfx::Mix(a, b, t); }
func Fade(int color, float alpha) -> int { return gfx::Fade(color, alpha); }
func Hex(string text) -> int { return gfx::Hex(text); }
func Red(int color) -> int { return gfx::Channel(color, 16); }
func Green(int color) -> int { return gfx::Channel(color, 8); }
func Blue(int color) -> int { return gfx::Channel(color, 0); }
func LoadImage(string path) -> int { return gfx::LoadImage(path); }
func ImageWidth(int image) -> int { return gfx::ImageSize(image, 0); }
func ImageHeight(int image) -> int { return gfx::ImageSize(image, 1); }
func Image(int image, float x, float y) { gfx::Image(image, x, y); }
func ImageScaled(int image, float x, float y, float width, float height) { gfx::Image(image, x, y, width, height); }
func ImagePart(int image, float sx, float sy, float sw, float sh, float x, float y, float scale) { gfx::ImagePart(image, sx, sy, sw, sh, x, y, scale); }
func Capture(float x, float y, float width, float height) -> int { return gfx::Capture(x, y, width, height); }
func Save(string path) -> bool { return gfx::Save(path); }
func Checksum() -> int { return gfx::Checksum(); }
)clp";

}  // namespace

// --- shared with the other host libraries -------------------------------------------------------

Canvas& canvas() { return g_canvas; }

void set_clip(const double x, const double y, const double w, const double h) {
  g_clip = Clip{static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::ceil(x + w)),
                static_cast<int>(std::ceil(y + h))};
}

void clear_clip() { g_clip = Clip{}; }

int load_image_file(const std::string& path, std::string& error) {
  static std::unordered_map<std::string, int> cache;
  const auto found = cache.find(path);
  if (found != cache.end()) {
    return found->second;
  }
  const int handle = load_image(path, error);
  if (handle != 0) {
    cache.emplace(path, handle);
  }
  return handle;
}

bool image_dimensions(const int handle, int& width, int& height) {
  std::string ignored;
  const Image* image = image_at(handle, ignored);
  if (image == nullptr) {
    return false;
  }
  width = image->width;
  height = image->height;
  return true;
}

void draw_image_box(const int handle, const double x, const double y, const double w, const double h) {
  std::string ignored;
  if (const Image* image = image_at(handle, ignored)) {
    draw_image(*image, 0, 0, image->width, image->height, x, y, w, h);
  }
}

void blur_area(const double x, const double y, const double w, const double h, const double radius) {
  const Clip saved = g_clip;
  g_clip = Clip{std::max(saved.x0, static_cast<int>(std::floor(x))), std::max(saved.y0, static_cast<int>(std::floor(y))),
                std::min(saved.x1, static_cast<int>(std::ceil(x + w))), std::min(saved.y1, static_cast<int>(std::ceil(y + h)))};
  std::string ignored;
  (void)apply_effect("blur", radius, ignored);
  g_clip = saved;
}

std::vector<std::uint32_t> read_area(const int x, const int y, const int w, const int h) {
  std::vector<std::uint32_t> pixels(static_cast<std::size_t>(std::max(0, w)) * static_cast<std::size_t>(std::max(0, h)), 0);
  for (int row = 0; row < h; ++row) {
    for (int column = 0; column < w; ++column) {
      const int px = x + column;
      const int py = y + row;
      if (px >= 0 && py >= 0 && px < g_canvas.width && py < g_canvas.height) {
        pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(w) + static_cast<std::size_t>(column)] =
            g_canvas.pixels[static_cast<std::size_t>(py) * static_cast<std::size_t>(g_canvas.width) + static_cast<std::size_t>(px)];
      }
    }
  }
  return pixels;
}

void blend_area(const int x, const int y, const int w, const int h, const std::vector<std::uint32_t>& before,
                const double amount_new) {
  const double keep = 1.0 - saturate(amount_new);
  for (int row = 0; row < h; ++row) {
    for (int column = 0; column < w; ++column) {
      const int px = x + column;
      const int py = y + row;
      if (px >= 0 && py >= 0 && px < g_canvas.width && py < g_canvas.height) {
        blend(g_canvas.pixels[static_cast<std::size_t>(py) * static_cast<std::size_t>(g_canvas.width) + static_cast<std::size_t>(px)],
              before[static_cast<std::size_t>(row) * static_cast<std::size_t>(w) + static_cast<std::size_t>(column)] & 0xFFFFFFU,
              keep);
      }
    }
  }
}

int add_image(const int width, const int height, std::vector<std::uint32_t> pixels) {
  Image image;
  image.width = width;
  image.height = height;
  image.pixels = std::move(pixels);
  g_images.push_back(std::move(image));
  return static_cast<int>(g_images.size());
}

void resize_canvas(const int width, const int height) {
  g_canvas.width = width;
  g_canvas.height = height;
  g_canvas.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0xFF000000U);
}

std::uint32_t mix_color(const std::uint32_t a, const std::uint32_t b, const double t) {
  std::uint32_t result = 0;
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    const double from = static_cast<double>((a >> shift) & 0xFFU);
    const double to = static_cast<double>((b >> shift) & 0xFFU);
    result |= static_cast<std::uint32_t>(std::lround(from + (to - from) * t)) << shift;
  }
  return result;
}

void fill_rect(double x, double y, double w, double h, const std::uint32_t color) {
  if (w < 0) {
    x += w;
    w = -w;
  }
  if (h < 0) {
    y += h;
    h = -h;
  }
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const int x0 = clamp_int(std::round(x), area.x0, area.x1);
  const int x1 = clamp_int(std::round(x + w), area.x0, area.x1);
  const int y0 = clamp_int(std::round(y), area.y0, area.y1);
  const int y1 = clamp_int(std::round(y + h), area.y0, area.y1);
  for (int py = y0; py < y1; ++py) {
    std::uint32_t* row = g_canvas.pixels.data() + static_cast<std::size_t>(py) * static_cast<std::size_t>(g_canvas.width);
    for (int px = x0; px < x1; ++px) {
      blend(row[px], paint.rgb, paint.alpha);
    }
  }
}

void stroke_rect(const double x, const double y, const double w, const double h, const double thickness,
                 const std::uint32_t color) {
  const double t = std::max(1.0, thickness);
  fill_rect(x, y, w, t, color);
  fill_rect(x, y + h - t, w, t, color);
  fill_rect(x, y + t, t, h - 2 * t, color);
  fill_rect(x + w - t, y + t, t, h - 2 * t, color);
}

void fill_round_rect(const double x, const double y, const double w, const double h, double radius,
                     const std::uint32_t color) {
  if (!(w > 0) || !(h > 0)) {
    return;
  }
  radius = std::clamp(radius, 0.0, std::min(w, h) / 2.0);
  if (radius < 0.5) {
    fill_rect(x, y, w, h, color);
    return;
  }
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const double cx = x + w / 2.0;
  const double cy = y + h / 2.0;
  const double hx = w / 2.0 - radius;
  const double hy = h / 2.0 - radius;
  const int x0 = clamp_int(std::floor(x), area.x0, area.x1);
  const int x1 = clamp_int(std::ceil(x + w), area.x0, area.x1);
  const int y0 = clamp_int(std::floor(y), area.y0, area.y1);
  const int y1 = clamp_int(std::ceil(y + h), area.y0, area.y1);
  for (int py = y0; py < y1; ++py) {
    const double qy = std::max(std::fabs(py + 0.5 - cy) - hy, 0.0);
    for (int px = x0; px < x1; ++px) {
      const double qx = std::max(std::fabs(px + 0.5 - cx) - hx, 0.0);
      double coverage = 1.0;
      if (qx > 0 && qy > 0) {
        coverage = saturate(radius + 0.5 - std::sqrt(qx * qx + qy * qy));
      } else {
        // straight edges: partial coverage of the outermost pixel row/column
        coverage = saturate(radius + 0.5 - std::max(qx, qy));
      }
      if (coverage > 0) {
        plot(px, py, paint, coverage, area);
      }
    }
  }
}

void fill_circle(const double cx, const double cy, const double r, const std::uint32_t color) {
  if (r <= 0) {
    return;
  }
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const int left = clamp_int(std::floor(cx - r - 1), area.x0, area.x1);
  const int right = clamp_int(std::ceil(cx + r + 1), area.x0, area.x1);
  const int top = clamp_int(std::floor(cy - r - 1), area.y0, area.y1);
  const int bottom = clamp_int(std::ceil(cy + r + 1), area.y0, area.y1);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const double dx = x + 0.5 - cx;
      const double dy = y + 0.5 - cy;
      const double coverage = saturate(r + 0.5 - std::sqrt(dx * dx + dy * dy));
      if (coverage > 0) {
        plot(x, y, paint, coverage, area);
      }
    }
  }
}

void stroke_line(const double x1, const double y1, const double x2, const double y2, const double thickness,
                 const std::uint32_t color) {
  draw_line(x1, y1, x2, y2, thickness, color);
}

double text_height(const double size) { return text::line_height(size); }

double text_width(const std::string_view text, const double size) { return text::width(text, size); }

void draw_text(const std::string_view text, const double x, const double y, const double size, const std::uint32_t color) {
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  text::draw(text, x, y, size, [&](const int px, const int py, const double coverage) { plot(px, py, paint, coverage, area); });
}

void stroke_round_rect(const double x, const double y, const double w, const double h, double radius,
                       const double thickness, const std::uint32_t color) {
  if (!(w > 0) || !(h > 0)) {
    return;
  }
  radius = std::clamp(radius, 0.0, std::min(w, h) / 2.0);
  const double t = std::max(0.5, thickness);
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const double cx = x + w / 2.0;
  const double cy = y + h / 2.0;
  const int x0 = clamp_int(std::floor(x - 1), area.x0, area.x1);
  const int x1 = clamp_int(std::ceil(x + w + 1), area.x0, area.x1);
  const int y0 = clamp_int(std::floor(y - 1), area.y0, area.y1);
  const int y1 = clamp_int(std::ceil(y + h + 1), area.y0, area.y1);
  for (int py = y0; py < y1; ++py) {
    for (int px = x0; px < x1; ++px) {
      const double qx = std::fabs(px + 0.5 - cx) - (w / 2.0 - radius);
      const double qy = std::fabs(py + 0.5 - cy) - (h / 2.0 - radius);
      const double outside = std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) + std::min(std::max(qx, qy), 0.0) - radius;
      // the stroke sits just inside the edge: distance from the band [-t, 0]
      const double band = std::fabs(outside + t / 2.0) - t / 2.0;
      const double coverage = saturate(0.5 - band);
      if (coverage > 0) {
        plot(px, py, paint, coverage, area);
      }
    }
  }
}

void fill_shadow(const double x, const double y, const double w, const double h, double radius, double blur,
                 const std::uint32_t color) {
  if (!(w > 0) || !(h > 0)) {
    return;
  }
  blur = std::max(1.0, blur);
  radius = std::clamp(radius, 0.0, std::min(w, h) / 2.0);
  const Clip area = bounds();
  const Paint paint = paint_of(color);
  const double cx = x + w / 2.0;
  const double cy = y + h / 2.0;
  const int x0 = clamp_int(std::floor(x - blur), area.x0, area.x1);
  const int x1 = clamp_int(std::ceil(x + w + blur), area.x0, area.x1);
  const int y0 = clamp_int(std::floor(y - blur), area.y0, area.y1);
  const int y1 = clamp_int(std::ceil(y + h + blur), area.y0, area.y1);
  for (int py = y0; py < y1; ++py) {
    for (int px = x0; px < x1; ++px) {
      const double qx = std::fabs(px + 0.5 - cx) - (w / 2.0 - radius);
      const double qy = std::fabs(py + 0.5 - cy) - (h / 2.0 - radius);
      const double distance = std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) + std::min(std::max(qx, qy), 0.0) - radius;
      const double t = saturate(0.5 - distance / blur);  // 1 inside, fades to 0 at `blur` outside
      const double coverage = t * t * (3 - 2 * t);
      if (coverage > 0.002) {
        plot(px, py, paint, coverage, area);
      }
    }
  }
}

void add_gfx(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"gfx::Canvas", 2, gfx_canvas, false},
                                {"gfx::Width", 0, gfx_width, false},
                                {"gfx::Height", 0, gfx_height, false},
                                {"gfx::Clear", 1, gfx_clear, false},
                                {"gfx::Pixel", 3, gfx_pixel, false},
                                {"gfx::GetPixel", 2, gfx_get_pixel, false},
                                {"gfx::Rect", 5, gfx_rect, false},
                                {"gfx::RectLine", 5, gfx_rect_line, false},
                                {"gfx::RectLine", 6, gfx_rect_line, false},
                                {"gfx::RoundRect", 6, gfx_round_rect, false},
                                {"gfx::Circle", 4, gfx_circle, false},
                                {"gfx::CircleLine", 4, gfx_circle_line, false},
                                {"gfx::CircleLine", 5, gfx_circle_line, false},
                                {"gfx::Line", 5, gfx_line, false},
                                {"gfx::Line", 6, gfx_line, false},
                                {"gfx::Triangle", 7, gfx_triangle, false},
                                {"gfx::Gradient", 6, gfx_gradient, false},
                                {"gfx::Gradient", 7, gfx_gradient, false},
                                {"gfx::Text", 5, gfx_text, false},
                                {"gfx::Font", 1, gfx_font, false},
                                {"gfx::Bold", 1, gfx_bold, false},
                                {"gfx::Shadow", 7, gfx_shadow, false},
                                {"gfx::RoundRectLine", 7, gfx_round_rect_line, false},
                                {"gfx::Effect", 2, gfx_effect, false},
                                {"gfx::Tint", 2, gfx_tint, false},
                                {"gfx::TextWidth", 2, gfx_text_width, false},
                                {"gfx::TextHeight", 1, gfx_text_height, false},
                                {"gfx::Clip", 4, gfx_clip, false},
                                {"gfx::NoClip", 0, gfx_no_clip, false},
                                {"gfx::Rgb", 3, gfx_rgb, false},
                                {"gfx::Rgb", 4, gfx_rgb, false},
                                {"gfx::Hsv", 3, gfx_hsv, false},
                                {"gfx::Mix", 3, gfx_mix, false},
                                {"gfx::Fade", 2, gfx_fade, false},
                                {"gfx::Hex", 1, gfx_hex, false},
                                {"gfx::Channel", 2, gfx_channel, false},
                                {"gfx::LoadImage", 1, gfx_load_image, true},
                                {"gfx::ImageSize", 2, gfx_image_size, false},
                                {"gfx::Image", 3, gfx_image, false},
                                {"gfx::Image", 5, gfx_image, false},
                                {"gfx::ImagePart", 8, gfx_image_part, false},
                                {"gfx::Capture", 4, gfx_capture, false},
                                {"gfx::Save", 1, gfx_save, true},
                                {"gfx::Checksum", 0, gfx_checksum, false},
                            });
}

std::string_view gfx_source() { return kGfxSource; }

}  // namespace clpp::stdlib::host
