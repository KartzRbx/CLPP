#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace clpp::detail {

// C++20 path construction with explicit UTF-8, including Windows native paths.
[[nodiscard]] inline std::filesystem::path utf8_path(const std::string_view text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

}  // namespace clpp::detail
