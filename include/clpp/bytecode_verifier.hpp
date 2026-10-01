#pragma once

#include "clpp/compiler.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace clpp {

enum class BytecodeError {
  Version,
  TooLarge,
  Decode,
  Entry,
  Opcode,
  Constant,
  Function,
  Local,
  Vtable,
  Type,
  Operand,
  Target,
  StackUnderflow,
  StackMismatch,
};

struct BytecodeDiagnostic {
  BytecodeError code{BytecodeError::Version};
  std::size_t pc{0};
  int instruction{-1};
  SourceLocation location{};
  std::string message;
};

struct BytecodeReport {
  std::vector<BytecodeDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
  [[nodiscard]] std::string message() const {
    return diagnostics.empty() ? std::string{} : diagnostics.front().message;
  }
};

[[nodiscard]] BytecodeReport verify_bytecode_report(const BytecodeChunk& chunk);

// Empty when the chunk is structurally valid. The string is the first diagnostic message.
[[nodiscard]] std::string verify_bytecode(const BytecodeChunk& chunk);

[[nodiscard]] SourceLocation source_location_at(const BytecodeChunk& chunk, std::size_t pc);

}  // namespace clpp
