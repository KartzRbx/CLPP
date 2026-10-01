#include "clpp/stdlib.hpp"

#include <string>

namespace clpp::stdlib {

std::string http_host(const std::string_view url) {
  const std::size_t scheme = url.find("://");
  const std::size_t start = scheme == std::string_view::npos ? 0 : scheme + 3;
  const std::size_t slash = url.find('/', start);
  std::size_t end = slash == std::string_view::npos ? url.size() : slash;
  const std::size_t colon = url.find(':', start);
  if (colon != std::string_view::npos && colon < end) {
    end = colon;
  }
  return std::string(url.substr(start, end - start));
}

}  // namespace clpp::stdlib
