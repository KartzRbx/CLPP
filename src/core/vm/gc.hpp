#pragma once

#include "clpp/value.hpp"

#include <memory>
#include <vector>

namespace clpp::vm {

void mark_value(Value& value);
void sweep(std::vector<std::unique_ptr<Table>>& heap);
[[nodiscard]] Table* allocate(std::vector<std::unique_ptr<Table>>& heap);

}  // namespace clpp::vm
