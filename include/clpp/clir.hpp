#pragma once

#include "clpp/analyze.hpp"
#include "clpp/compiler.hpp"
#include "clpp/register_ir.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace clpp {

struct ClirEmitResult {
  BytecodeChunk chunk;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

[[nodiscard]] ClirEmitResult emit_clir(const AnalysisResult& analysis);

[[nodiscard]] RegChunk lower_to_vm(const BytecodeChunk& chunk);

// Test serializer for CLIR v1. Not a stable distribution format.
[[nodiscard]] std::string serialize_clir(const BytecodeChunk& chunk);
[[nodiscard]] BytecodeChunk deserialize_clir(std::string_view bytes);

enum class Backend { Vm, Luau };

// Empty when every opcode in the chunk has a lowering on that backend.
[[nodiscard]] std::string check_backend(Backend backend, const BytecodeChunk& chunk);

struct LuauEmit {
  std::string source;
  std::string error;
};

// Text backend for a small statement subset. Does not invoke a Luau VM.
[[nodiscard]] LuauEmit clir_to_luau(const AnalysisResult& analysis);

}  // namespace clpp
