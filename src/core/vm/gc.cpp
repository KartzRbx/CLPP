#include "core/vm/gc.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_set>

namespace clpp::vm {

void mark_value(Value& value) {
  std::vector<Value> pending{value};
  std::unordered_set<const void*> visited;
  while (!pending.empty()) {
    Value current = std::move(pending.back()); pending.pop_back();
    if ((current.is_struct() || current.is_task() || current.is_closure() || current.is_cell() || current.is_resource()) && visited.insert(current.identity()).second) {
      for (auto& field : current.snapshot_fields()) pending.push_back(std::move(field));
    }
    if (!current.is_table() || current.table == nullptr || current.table->marked) continue;
    current.table->marked = true;
    for (auto& entry : current.table->entries) pending.push_back(entry.second);
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
