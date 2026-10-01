#pragma once

#include "clpp/value.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::stdlib {

[[nodiscard]] std::optional<std::string> module_source(std::string_view path);

// Modules reachable with `link @clpp.<name> as Alias;`, for editor completion and docs.
struct ModuleEntry {
  std::string_view path;         // "@clpp.fs"
  std::string_view alias;        // suggested alias, "Fs"
  std::string_view description;  // one line
};
[[nodiscard]] const std::vector<ModuleEntry>& module_catalog();
[[nodiscard]] double file_size(std::string_view path);
[[nodiscard]] std::string env_value(std::string_view name);
[[nodiscard]] std::string http_host(std::string_view url);
[[nodiscard]] std::string_view axiom_source();
[[nodiscard]] int axiom_native(std::string_view name, std::size_t arity);
[[nodiscard]] bool axiom_apply(std::uint8_t id, const Value* args, std::uint8_t arity, Value& out, std::string& error);
void set_program_args(const std::vector<std::string>& args);
[[nodiscard]] bool std_apply(std::uint8_t id, const Value* args, std::uint8_t arity, Value& out, std::string& error);

}  // namespace clpp::stdlib
