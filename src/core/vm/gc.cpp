#include "core/vm/gc.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace clpp::vm {

void mark_value(Value& value) {
  if (value.is_struct() || value.is_task()) {
    for (Value& field : value.fields) {
      mark_value(field);
    }
  }
  if (!value.is_table() || value.table == nullptr || value.table->marked) {
    return;
  }
  value.table->marked = true;
  for (std::pair<std::string, Value>& entry : value.table->entries) {
    mark_value(entry.second);
  }
}

void sweep(std::vector<std::unique_ptr<Table>>& heap) {
  heap.erase(std::remove_if(heap.begin(), heap.end(),
                            [](const std::unique_ptr<Table>& object) { return object == nullptr || !object->marked; }),
             heap.end());
}

Table* allocate(std::vector<std::unique_ptr<Table>>& heap) {
  heap.push_back(std::make_unique<Table>());
  return heap.back().get();
}

}  // namespace clpp::vm
