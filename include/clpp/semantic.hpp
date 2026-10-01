#pragma once

#include "clpp/analyze.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

struct SemanticSymbol {
  std::uint32_t id{0};
  std::uint32_t scope{0};
  std::uint16_t slot{0};
  std::string name;
  SourceLocation location{};
  bool declaration{false};
};

struct SemanticSnapshot {
  std::vector<SemanticSymbol> symbols;
};

[[nodiscard]] SemanticSnapshot build_snapshot(const AnalysisResult& analysis);

// Symbols that share the declaration id under the cursor, including the declaration.
[[nodiscard]] std::vector<SemanticSymbol> references_at(const SemanticSnapshot& snapshot, std::uint32_t line,
                                                        std::uint32_t column);

// LSP positions are UTF-16 code units. Lexer columns are 1-based bytes.
[[nodiscard]] int byte_column_from_utf16(std::string_view text, int lsp_line, int lsp_character);
[[nodiscard]] int lsp_character_at(std::string_view text, int line1, int column1);

}  // namespace clpp
