// This root allocates after main, when function-local runtime helpers may already be destroyed.
#include "clpp/value.hpp"
#include <cstdlib>
struct LateAllocation {
  ~LateAllocation() {
    auto value = clpp::Value::struct_of({clpp::Value::integer_of(42)});
    value.append_field(value);
    value.freeze();
    if (value.field(0).integer != 42 || !value.frozen()) std::abort();
    clpp::collect_shared_heap();
  }
} late;
int main() {
  auto value = clpp::Value::struct_of({clpp::Value::null_of()});
  value.set_field(0, value);
  value = clpp::Value::null_of();
  return clpp::shared_heap_stats().objects == 0 ? 0 : 1;
}
