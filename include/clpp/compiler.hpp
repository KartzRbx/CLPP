#pragma once

#include "clpp/core/lexer/token.hpp"
#include "clpp/value.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

inline constexpr std::uint16_t kBytecodeFormatVersion = 13;

struct FunctionBytecode {
  std::uint16_t arity{0};
  std::uint16_t local_count{0};
  std::uint16_t entry{0};
  bool is_async{false};
};

struct SourceMapEntry {
  std::uint16_t pc{0};
  SourceLocation location{};
};

struct BytecodeChunk {
  std::vector<std::uint8_t> code;
  std::vector<Value> constants;
  std::vector<FunctionBytecode> functions;
  std::vector<std::vector<std::uint16_t>> vtables;
  std::vector<SourceMapEntry> source_map;
  // Paths are relative to the entry source; native modules retain @clpp names.
  std::vector<std::string> source_files;
  std::uint16_t local_count{0};
  std::uint16_t entry{0};
  std::uint16_t version{kBytecodeFormatVersion};
};

// CLIR is the in-memory stack chunk. See docs/reference/toolchain.md for compatibility.
using ClirChunk = BytecodeChunk;

struct CompileResult {
  BytecodeChunk chunk;
  std::vector<Diagnostic> diagnostics;
  // Shared immutable storage keeps diagnostic spans valid across result copies and moves.
  std::shared_ptr<const std::vector<std::string>> diagnostic_text;

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

using ModuleLoader = std::function<std::optional<std::string>(std::string_view path)>;

class Compiler {
 public:
  [[nodiscard]] CompileResult compile(std::string_view source, const ModuleLoader& modules = {}) const;
};

}  // namespace clpp
