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

// Host libraries: natives that talk to the operating system or keep state between calls
// (@clpp.window, @clpp.gfx, @clpp.ui, @clpp.json, @clpp.time and the writing half of @clpp.fs).
// Their module sources call `lib::Name(...)`; the binder turns that into 2000 + id.
[[nodiscard]] int host_native(std::string_view name, std::size_t arity);
[[nodiscard]] bool host_apply(std::uint8_t id, const Value* args, std::uint8_t arity, Value& out, std::string& error);
// True for natives refused inside actor(...): files, windows, input, the clock.
[[nodiscard]] bool host_sandboxed(std::uint8_t id);
// Names of every host native ("window::Open"), for tests and documentation checks.
[[nodiscard]] std::vector<std::string> host_native_names();
[[nodiscard]] bool std_apply(std::uint8_t id, const Value* args, std::uint8_t arity, Value& out, std::string& error);

}  // namespace clpp::stdlib
