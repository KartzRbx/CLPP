#pragma once

#include "clpp/bytecode_verifier.hpp"

#include <string>
#include <vector>

namespace clpp::compiler::bytecode {
struct DecodedChunk;
}

namespace clpp {

struct StackCheck {
  std::string error;
  BytecodeError code{BytecodeError::StackUnderflow};
  std::size_t pc{0};
  std::vector<int> depth;
};

// Stack depth for the VM backend. Structural checks are separate.
[[nodiscard]] StackCheck verify_stack(const BytecodeChunk& chunk, const compiler::bytecode::DecodedChunk& decoded);

}  // namespace clpp
