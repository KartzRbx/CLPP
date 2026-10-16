#include "clpp/stdlib.hpp"

#include <cstdlib>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#endif

namespace clpp::stdlib {

std::string env_value(const std::string_view name) {
#ifdef _WIN32
  if (name.empty() || name.find('\0') != std::string_view::npos || name.size() > 32766) return {};
  const int name_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0);
  if (name_size == 0) return {};
  std::wstring key(static_cast<std::size_t>(name_size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), key.data(), name_size);
  const DWORD size = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
  if (size == 0 || size > 2147483647) return {};
  std::vector<wchar_t> value(size);
  const DWORD count = GetEnvironmentVariableW(key.c_str(), value.data(), size);
  if (count == 0 || count >= size) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(count), nullptr, 0, nullptr, nullptr);
  if (bytes == 0) return {};
  std::string output(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(count), output.data(), bytes, nullptr, nullptr);
  return output;
#else
  const char* const value = std::getenv(std::string(name).c_str());
  if (value == nullptr) {
    return {};
  }
  return std::string(value);
#endif
}

}  // namespace clpp::stdlib
