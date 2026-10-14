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
  std::shared_ptr<const std::vector<std::string>> diagnostic_text;

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

[[nodiscard]] ClirEmitResult emit_clir(const AnalysisResult& analysis);

[[nodiscard]] RegChunk lower_to_vm(const BytecodeChunk& chunk);

// Complete test container for CLIR bytecode version 2. The container revision is separate from the
// bytecode version; this is not a stable distribution format. Invalid input returns
// an empty chunk with version 0. Runtime heap values are not constant-pool literals.
inline constexpr std::uint16_t kClirContainerVersion = 3;
[[nodiscard]] std::string serialize_clir(const BytecodeChunk& chunk);
[[nodiscard]] BytecodeChunk deserialize_clir(std::string_view bytes);

}  // namespace clpp
