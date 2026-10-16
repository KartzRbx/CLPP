#pragma once

#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#ifdef __APPLE__
#include <stdlib.h>
#include <xlocale.h>
#endif

namespace clpp::stdlib::detail {

// Classic locale is local to this stream; parsing never changes global locale.
inline bool parse_decimal_classic(const std::string_view text, double& value) {
  if (text.empty()) return false;
#ifdef __APPLE__
  // Older Apple num_get marks representable subnormals as stream failure.
  // Use a private C numeric locale; ERANGE alone does not imply a zero result.
  const std::string owned(text);
  locale_t numeric = newlocale(LC_NUMERIC_MASK, "C", nullptr);
  if (!numeric) return false;
  char* end = nullptr;
  value = strtod_l(owned.c_str(), &end, numeric);
  freelocale(numeric);
  if (end == owned.c_str() || end != owned.c_str() + owned.size() || !std::isfinite(value)) return false;
#else
  std::istringstream input{std::string(text)};
  input.imbue(std::locale::classic());
  input >> std::noskipws >> value;
  if (!input || input.peek() != std::char_traits<char>::eof() || !std::isfinite(value)) return false;
#endif
  if (value == 0) {
    for (const char byte : text) {
      if (byte == 'e' || byte == 'E') break;
      if (byte >= '1' && byte <= '9') return false;  // Nonzero value underflowed to zero.
    }
  }
  return true;
}

template <typename Float>
inline bool parse_decimal(const std::string_view text, Float& value) {
  if constexpr (requires(const char* begin, const char* end, Float& result) {
    std::from_chars(begin, end, result, std::chars_format::general);
  }) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value);
  } else {
    return parse_decimal_classic(text, value);
  }
}

}  // namespace clpp::stdlib::detail
