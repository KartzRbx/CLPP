#pragma once

#include "clpp/compiler.hpp"
#include "core/compiler/opcode.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace clpp::compiler::bytecode {

struct Instruction {
  std::size_t pc{0};
  Opcode op{Opcode::Nop};
  std::uint16_t imm{0};
  std::uint16_t aux{0};
  std::uint8_t extra{0};
};

struct DecodedChunk {
  std::vector<Instruction> instructions;
  std::vector<int> instruction_at;
};

[[nodiscard]] bool decode(const std::vector<std::uint8_t>& code, DecodedChunk& decoded);

[[nodiscard]] int target_of(const std::vector<int>& instruction_at, std::uint16_t byte);

}  // namespace clpp::compiler::bytecode