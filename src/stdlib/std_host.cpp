// Registry of the host libraries (see host.hpp).

#include "clpp/stdlib.hpp"
#include "stdlib/host.hpp"

#include <string>
#include <vector>

namespace clpp::stdlib {

namespace {

[[nodiscard]] const std::vector<host::Entry>& registry() {
  static const std::vector<host::Entry> table = [] {
    std::vector<host::Entry> entries;
    host::add_gfx(entries);
    host::add_window(entries);
    host::add_ui(entries);
    host::add_json(entries);
    host::add_time(entries);
    host::add_files(entries);
    host::add_io(entries);
    host::add_gui(entries);
    host::add_audio(entries);
    host::add_input(entries);
    return entries;
  }();
  return table;
}

}  // namespace

int host_native(const std::string_view name, const std::size_t arity) {
  const std::vector<host::Entry>& table = registry();
  for (std::size_t index = 0; index < table.size() && index < 256; ++index) {
    if (table[index].arity == arity && table[index].name == name) {
      return 2000 + static_cast<int>(index);
    }
  }
  return -1;
}

bool host_apply(const std::uint8_t id, const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  const std::vector<host::Entry>& table = registry();
  if (id >= table.size() || table[id].arity != arity) {
    error = "runtime error";
    return false;
  }
  out = Value::number_of(0);
  return table[id].function(args, arity, out, error);
}

bool host_sandboxed(const std::uint8_t id) {
  const std::vector<host::Entry>& table = registry();
  return id >= table.size() || table[id].sandboxed;
}

std::vector<std::string> host_native_names() {
  std::vector<std::string> names;
  for (const host::Entry& entry : registry()) {
    names.emplace_back(entry.name);
  }
  return names;
}

}  // namespace clpp::stdlib
