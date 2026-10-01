#include "clpp/stdlib.hpp"

#include <cstdlib>
#include <string>

namespace clpp::stdlib {

std::string env_value(const std::string_view name) {
  const char* const value = std::getenv(std::string(name).c_str());
  if (value == nullptr) {
    return {};
  }
  return std::string(value);
}

}  // namespace clpp::stdlib
