#pragma once

#include "clpp/core/lexer/token.hpp"
#include "clpp/value.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

inline constexpr std::uint16_t kBytecodeFormatVersion = 1;

struct FunctionBytecode {
  std::uint16_t arity{0};
  std::uint16_t local_count{0};
  std::uint16_t entry{0};
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
  std::uint16_t local_count{0};
  std::uint16_t entry{0};
  std::uint16_t version{kBytecodeFormatVersion};
};

// CLIR v1 is the in-memory stack chunk. See docs/clir-v1.md.
using ClirChunk = BytecodeChunk;

struct CompileResult {
  BytecodeChunk chunk;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

using ModuleLoader = std::function<std::optional<std::string>(std::string_view path)>;

class Compiler {
 public:
  [[nodiscard]] CompileResult compile(std::string_view source, const ModuleLoader& modules = {}) const;
};

}  // namespace clpp
