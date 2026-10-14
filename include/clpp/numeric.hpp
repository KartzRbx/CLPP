#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace clpp::numeric {
inline constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
inline constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

inline std::optional<std::int64_t> parse_integer(std::string_view text, bool negative = false) {
  std::string body;
  for (const char c : text) if (c != '_') body.push_back(c);
  int base = 10;
  std::size_t start = 0;
  if (body.size() > 2 && body[0] == '0') {
    if (body[1] == 'x' || body[1] == 'X') { base = 16; start = 2; }
    else if (body[1] == 'b' || body[1] == 'B') { base = 2; start = 2; }
  }
  std::uint64_t magnitude = 0;
  const auto result = std::from_chars(body.data() + start, body.data() + body.size(), magnitude, base);
  if (result.ec != std::errc{} || result.ptr != body.data() + body.size()) return std::nullopt;
  const auto limit = static_cast<std::uint64_t>(maximum) + (negative ? 1 : 0);
  if (magnitude > limit) return std::nullopt;
  if (negative && magnitude == limit) return minimum;
  const auto value = static_cast<std::int64_t>(magnitude);
  return negative ? -value : value;
}

inline std::optional<std::int64_t> from_float(const double value) {
  // The upper bound is exclusive: INT64_MAX rounds to 2^63 as a double.
  if (!std::isfinite(value) || value < -0x1p63 || value >= 0x1p63) return std::nullopt;
  return static_cast<std::int64_t>(std::trunc(value));
}

inline std::optional<std::int64_t> add(const std::int64_t a, const std::int64_t b) {
  if ((b > 0 && a > maximum - b) || (b < 0 && a < minimum - b)) return std::nullopt;
  return a + b;
}
inline std::optional<std::int64_t> subtract(const std::int64_t a, const std::int64_t b) {
  if ((b > 0 && a < minimum + b) || (b < 0 && a > maximum + b)) return std::nullopt;
  return a - b;
}
inline std::optional<std::int64_t> multiply(const std::int64_t a, const std::int64_t b) {
  if (a > 0 ? (b > 0 ? a > maximum / b : b < minimum / a)
            : (a < 0 && (b > 0 ? a < minimum / b : b < 0 && a < maximum / b))) return std::nullopt;
  return a * b;
}
inline std::optional<std::int64_t> divide(const std::int64_t a, const std::int64_t b) {
  if (b == 0 || (a == minimum && b == -1)) return std::nullopt;
  return a / b;
}
inline std::optional<std::int64_t> remainder(const std::int64_t a, const std::int64_t b) {
  if (b == 0) return std::nullopt;
  if (a == minimum && b == -1) return 0;
  return a % b;
}
// Compare an exact integer with a floating value without rounding the integer.
inline std::optional<int> compare(const std::int64_t integer, const double floating) {
  if (std::isnan(floating)) return std::nullopt;
  if (floating >= 0x1p63) return -1;
  if (floating < -0x1p63) return 1;
  const auto truncated = static_cast<std::int64_t>(floating);
  if (integer < truncated) return -1;
  if (integer > truncated) return 1;
  const double whole = std::trunc(floating);
  return floating > whole ? -1 : floating < whole ? 1 : 0;
}
}  // namespace clpp::numeric
