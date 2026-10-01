#include "core/compiler/bytecode_format.hpp"

namespace clpp::compiler::bytecode {

bool decode(const std::vector<std::uint8_t>& code, DecodedChunk& decoded) {
  decoded.instructions.clear();
  decoded.instruction_at.assign(code.size() + 1, -1);
  std::size_t pc = 0;
  while (pc < code.size()) {
    decoded.instruction_at[pc] = static_cast<int>(decoded.instructions.size());
    Instruction instruction;
    instruction.pc = pc;
    instruction.op = static_cast<Opcode>(code[pc++]);
    const auto take_u16 = [&](std::uint16_t& out) {
      if (pc + 1 >= code.size()) {
        return false;
      }
      out = static_cast<std::uint16_t>(code[pc] | (static_cast<std::uint16_t>(code[pc + 1]) << 8));
      pc += 2;
      return true;
    };
    switch (instruction.op) {
      case Opcode::Const:
      case Opcode::LoadLocal:
      case Opcode::StoreLocal:
      case Opcode::Jump:
      case Opcode::JumpIfFalse:
      case Opcode::Call:
      case Opcode::Schedule:
      case Opcode::SpawnThread:
      case Opcode::CCall:
      case Opcode::Tag:
      case Opcode::VCall:
      case Opcode::CoCreate:
      case Opcode::Defer:
      case Opcode::Actor:
      case Opcode::Axiom:
      case Opcode::Std:
      case Opcode::Host:
      case Opcode::MakeStruct:
      case Opcode::GetSelf:
      case Opcode::SetSelf:
      case Opcode::Parallel:
      case Opcode::Protect:
      case Opcode::ClearLocal:
      case Opcode::SelfBack:
      case Opcode::ListPush:
      case Opcode::ListPop:
      case Opcode::ListInsert:
      case Opcode::ListRemove:
        if (!take_u16(instruction.imm)) {
          return false;
        }
        if (instruction.op == Opcode::VCall && !take_u16(instruction.aux)) {
          return false;
        }
        break;
      case Opcode::MakeVector:
      case Opcode::GetField:
      case Opcode::SetField:
        if (pc >= code.size()) {
          return false;
        }
        instruction.extra = code[pc++];
        break;
      default:
        break;
    }
    decoded.instructions.push_back(instruction);
  }
  decoded.instruction_at[code.size()] = static_cast<int>(decoded.instructions.size());
  return true;
}

int target_of(const std::vector<int>& instruction_at, const std::uint16_t byte) {
  if (byte >= instruction_at.size() || instruction_at[byte] < 0) {
    return -1;
  }
  return instruction_at[byte];
}

}  // namespace clpp::compiler::bytecode