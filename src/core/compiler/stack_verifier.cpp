#include "clpp/stack_verifier.hpp"

#include "core/compiler/bytecode_format.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace clpp {

StackCheck verify_stack(const BytecodeChunk& chunk, const compiler::bytecode::DecodedChunk& decoded) {
  StackCheck checked;
  const std::vector<compiler::bytecode::Instruction>& insns = decoded.instructions;
  const std::vector<int>& at = decoded.instruction_at;
  checked.depth.assign(insns.size(), -1);

  std::vector<int> roots(chunk.code.size(), -1);
  const auto add_root = [&](const std::uint16_t byte, const std::uint16_t local_count) {
    if (byte < roots.size()) {
      roots[byte] = std::max(roots[byte], static_cast<int>(local_count));
    }
  };
  for (const FunctionBytecode& function : chunk.functions) {
    add_root(function.entry, function.local_count);
  }
  add_root(chunk.entry, chunk.local_count);

  std::vector<std::size_t> work;
  const auto plant = [&](const int index, const int sp) {
    if (sp < 0) {
      checked.error = "register stack underflow";
      checked.code = BytecodeError::StackUnderflow;
      return false;
    }
    if (index < 0 || static_cast<std::size_t>(index) >= insns.size()) {
      return true;
    }
    if (checked.depth[static_cast<std::size_t>(index)] < 0) {
      checked.depth[static_cast<std::size_t>(index)] = sp;
      work.push_back(static_cast<std::size_t>(index));
      return true;
    }
    if (checked.depth[static_cast<std::size_t>(index)] != sp) {
      checked.error = "register mismatch";
      checked.code = BytecodeError::StackMismatch;
      checked.pc = insns[static_cast<std::size_t>(index)].pc;
      return false;
    }
    return true;
  };

  for (std::size_t byte = 0; byte < roots.size(); ++byte) {
    if (roots[byte] >= 0 &&
        !plant(compiler::bytecode::target_of(at, static_cast<std::uint16_t>(byte)), roots[byte])) {
      if (checked.error.empty()) {
        checked.error = "register mismatch";
        checked.code = BytecodeError::StackMismatch;
      }
      return checked;
    }
  }

  for (std::size_t cursor = 0; cursor < work.size(); ++cursor) {
    const std::size_t index = work[cursor];
    const compiler::bytecode::Instruction& insn = insns[index];
    const int sp = checked.depth[index];
    const auto next_at = [&](const int next_sp) {
      if (index + 1 < insns.size() && !plant(static_cast<int>(index + 1), next_sp)) {
        return false;
      }
      return true;
    };
    bool ok = true;
    switch (insn.op) {
      case compiler::Opcode::Const:
      case compiler::Opcode::PushError:
      case compiler::Opcode::LoadLocal:
      case compiler::Opcode::GetSelf:
      case compiler::Opcode::Dup:
      case compiler::Opcode::Over:
      case compiler::Opcode::ListPop:
        ok = next_at(sp + 1);
        break;
      case compiler::Opcode::Post:
      case compiler::Opcode::Warn:
      case compiler::Opcode::Report:
      case compiler::Opcode::StoreLocal:
      case compiler::Opcode::Pop:
      case compiler::Opcode::SetSelf:
        ok = next_at(sp - 1);
        break;
      case compiler::Opcode::Add:
      case compiler::Opcode::BitAnd:
      case compiler::Opcode::BitOr:
      case compiler::Opcode::BitXor:
      case compiler::Opcode::Shl:
      case compiler::Opcode::Shr:
      case compiler::Opcode::Range:
      case compiler::Opcode::IntAdd:
      case compiler::Opcode::IntDiv:
      case compiler::Opcode::Sub:
      case compiler::Opcode::Mul:
      case compiler::Opcode::Div:
      case compiler::Opcode::Mod:
      case compiler::Opcode::Concat:
      case compiler::Opcode::Eq:
      case compiler::Opcode::NotEq:
      case compiler::Opcode::Less:
      case compiler::Opcode::LessEq:
      case compiler::Opcode::Greater:
      case compiler::Opcode::GreaterEq:
      case compiler::Opcode::GetIndex:
      case compiler::Opcode::ListFind:
      case compiler::Opcode::IterAt:
      case compiler::Opcode::SetField:
      case compiler::Opcode::ListPush:
        ok = next_at(sp - 1);
        break;
      case compiler::Opcode::BufferWrite:
      case compiler::Opcode::Slice:
      case compiler::Opcode::SetIndex:
      case compiler::Opcode::ListInsert:
        ok = next_at(sp - 2);
        break;
      case compiler::Opcode::JumpIfFalse: {
        const int landed = compiler::bytecode::target_of(at, insn.imm);
        ok = plant(landed, sp - 1) && next_at(sp - 1);
        break;
      }
      case compiler::Opcode::Jump:
        ok = plant(compiler::bytecode::target_of(at, insn.imm), sp);
        break;
      case compiler::Opcode::Protect:
        ok = plant(compiler::bytecode::target_of(at, insn.imm), sp) && next_at(sp);
        break;
      case compiler::Opcode::FetchAdd:
        ok = next_at(sp - 1);
        break;
      case compiler::Opcode::MutexNew:
        ok = next_at(sp + 1);
        break;
      case compiler::Opcode::CCall:
        ok = next_at(sp);
        break;
      case compiler::Opcode::Call:
      case compiler::Opcode::Schedule:
      case compiler::Opcode::SpawnThread: {
        if (insn.imm >= chunk.functions.size()) {
          checked.error = "register decode";
          checked.code = BytecodeError::Decode;
          checked.pc = insn.pc;
          return checked;
        }
        ok = next_at(sp - static_cast<int>(chunk.functions[insn.imm].arity) + 1);
        break;
      }
      case compiler::Opcode::VCall:
        ok = next_at(sp - static_cast<int>(insn.aux) + 1);
        break;
      case compiler::Opcode::CoCreate:
      case compiler::Opcode::Defer:
        ok = next_at(sp + 1);
        break;
      case compiler::Opcode::MakeVector:
        ok = next_at(sp - static_cast<int>(insn.extra) + 1);
        break;
      case compiler::Opcode::Axiom:
      case compiler::Opcode::Std:
      case compiler::Opcode::Host:
        ok = next_at(sp - static_cast<int>(insn.imm >> 8) + 1);
        break;
      case compiler::Opcode::MakeStruct:
      case compiler::Opcode::Parallel:
        ok = next_at(sp - static_cast<int>(insn.imm) + 1);
        break;
      case compiler::Opcode::Return:
      case compiler::Opcode::Halt:
        break;
      default:
        ok = next_at(sp);
        break;
    }
    if (!ok) {
      if (checked.error.empty()) {
        checked.error = "register mismatch";
        checked.code = BytecodeError::StackMismatch;
      }
      if (checked.pc == 0) {
        checked.pc = insn.pc;
      }
      return checked;
    }
  }
  return checked;
}

}  // namespace clpp
