#include "clpp/bytecode_verifier.hpp"

#include "core/compiler/bytecode_format.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace clpp {

SourceLocation source_location_at(const BytecodeChunk& chunk, const std::size_t pc) {
  SourceLocation location{};
  for (const SourceMapEntry& entry : chunk.source_map) {
    if (entry.pc > pc) {
      break;
    }
    location = entry.location;
  }
  return location;
}

namespace {

BytecodeDiagnostic make_error(const BytecodeChunk& chunk, const BytecodeError code, const std::size_t pc,
                              const int instruction, const std::string& message) {
  BytecodeDiagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.pc = pc;
  diagnostic.instruction = instruction;
  diagnostic.location = source_location_at(chunk, pc);
  diagnostic.message = message;
  return diagnostic;
}

}  // namespace

BytecodeReport verify_bytecode_report(const BytecodeChunk& chunk) {
  BytecodeReport report;
  const auto fail = [&](const BytecodeError code, const std::size_t pc, const int instruction, const char* message) {
    if (report.diagnostics.empty()) {
      report.diagnostics.push_back(make_error(chunk, code, pc, instruction, message));
    }
  };

  if (chunk.version != kBytecodeFormatVersion) {
    fail(BytecodeError::Version, 0, -1, "bytecode version");
    return report;
  }
  if (chunk.code.size() > 0xFFFF) {
    fail(BytecodeError::TooLarge, chunk.code.size(), -1, "register code too large");
    return report;
  }
  if (chunk.code.empty()) {
    if (!(chunk.functions.empty() && chunk.entry == 0)) {
      fail(BytecodeError::Entry, chunk.entry, -1, "register entry");
    }
    return report;
  }

  compiler::bytecode::DecodedChunk decoded;
  if (!compiler::bytecode::decode(chunk.code, decoded)) {
    fail(BytecodeError::Decode, 0, -1, "register decode");
    return report;
  }
  const std::vector<compiler::bytecode::Instruction>& instructions = decoded.instructions;
  const std::vector<int>& instruction_at = decoded.instruction_at;

  for (const std::vector<std::uint16_t>& vtable : chunk.vtables) {
    for (const std::uint16_t function : vtable) {
      if (function >= chunk.functions.size()) {
        fail(BytecodeError::Vtable, 0, -1, "register vtable");
        return report;
      }
    }
  }

  struct ScopeRoot {
    std::size_t instruction{0};
    int local_count{0};
  };
  std::vector<ScopeRoot> scope_roots;
  const auto add_root = [&](const std::uint16_t byte, const std::uint16_t local_count) {
    const int instruction = compiler::bytecode::target_of(instruction_at, byte);
    if (instruction < 0 || static_cast<std::size_t>(instruction) >= instructions.size()) {
      return false;
    }
    scope_roots.push_back(ScopeRoot{static_cast<std::size_t>(instruction), static_cast<int>(local_count)});
    return true;
  };
  for (const FunctionBytecode& function : chunk.functions) {
    if (!add_root(function.entry, function.local_count)) {
      fail(BytecodeError::Entry, function.entry, -1, "register entry");
      return report;
    }
  }
  if (!add_root(chunk.entry, chunk.local_count)) {
    fail(BytecodeError::Entry, chunk.entry, -1, "register entry");
    return report;
  }

  std::sort(scope_roots.begin(), scope_roots.end(),
            [](const ScopeRoot& left, const ScopeRoot& right) { return left.instruction < right.instruction; });
  std::vector<int> local_limit(instructions.size(), -1);
  std::vector<int> region(instructions.size(), -1);
  for (std::size_t root = 0, region_index = 0; root < scope_roots.size(); ++region_index) {
    const std::size_t start = scope_roots[root].instruction;
    int locals = scope_roots[root].local_count;
    std::size_t next = root + 1;
    while (next < scope_roots.size() && scope_roots[next].instruction == start) {
      locals = std::max(locals, scope_roots[next].local_count);
      ++next;
    }
    const std::size_t end = next < scope_roots.size() ? scope_roots[next].instruction : instructions.size();
    for (std::size_t instruction = start; instruction < end; ++instruction) {
      local_limit[instruction] = locals;
      region[instruction] = static_cast<int>(region_index);
    }
    root = next;
  }

  for (std::size_t index = 0; index < instructions.size(); ++index) {
    const compiler::bytecode::Instruction& instruction = instructions[index];
    const std::uint8_t opcode = static_cast<std::uint8_t>(instruction.op);
    if (opcode > static_cast<std::uint8_t>(compiler::Opcode::ListRemove) || opcode == 55) {
      fail(BytecodeError::Opcode, instruction.pc, static_cast<int>(index), "register opcode");
      return report;
    }
    switch (instruction.op) {
      case compiler::Opcode::Const:
        if (instruction.imm >= chunk.constants.size()) {
          fail(BytecodeError::Constant, instruction.pc, static_cast<int>(index), "register constant");
          return report;
        }
        break;
      case compiler::Opcode::LoadLocal:
      case compiler::Opcode::StoreLocal:
      case compiler::Opcode::ClearLocal:
        if (local_limit[index] < 0 || instruction.imm >= local_limit[index]) {
          fail(BytecodeError::Local, instruction.pc, static_cast<int>(index), "register local");
          return report;
        }
        break;
      case compiler::Opcode::Call:
      case compiler::Opcode::Schedule:
      case compiler::Opcode::SpawnThread:
      case compiler::Opcode::CoCreate:
      case compiler::Opcode::Defer:
      case compiler::Opcode::Actor:
        if (instruction.imm >= chunk.functions.size()) {
          fail(BytecodeError::Function, instruction.pc, static_cast<int>(index), "register function");
          return report;
        }
        break;
      case compiler::Opcode::GetSelf:
      case compiler::Opcode::SetSelf:
        if (instruction.imm >= chunk.constants.size() || !chunk.constants[instruction.imm].is_string()) {
          fail(BytecodeError::Constant, instruction.pc, static_cast<int>(index), "register constant");
          return report;
        }
        break;
      case compiler::Opcode::Tag:
        if (instruction.imm != 65535 && instruction.imm != 65534 &&
            (instruction.imm == 0 || static_cast<std::size_t>(instruction.imm - 1) >= chunk.vtables.size())) {
          fail(BytecodeError::Type, instruction.pc, static_cast<int>(index), "register type");
          return report;
        }
        break;
      case compiler::Opcode::MakeVector:
        if (instruction.extra < 2 || instruction.extra > 4) {
          fail(BytecodeError::Operand, instruction.pc, static_cast<int>(index), "register operand");
          return report;
        }
        break;
      case compiler::Opcode::Axiom:
      case compiler::Opcode::Std:
        if ((instruction.imm >> 8) > 8) {
          fail(BytecodeError::Operand, instruction.pc, static_cast<int>(index), "register operand");
          return report;
        }
        break;
      default:
        break;
    }

    if (instruction.op == compiler::Opcode::Jump || instruction.op == compiler::Opcode::JumpIfFalse ||
        instruction.op == compiler::Opcode::Protect) {
      const int target = compiler::bytecode::target_of(instruction_at, instruction.imm);
      if (target < 0 || static_cast<std::size_t>(target) >= instructions.size()) {
        fail(BytecodeError::Target, instruction.pc, static_cast<int>(index), "register target");
        return report;
      }
      if (region[index] >= 0 && region[static_cast<std::size_t>(target)] >= 0 &&
          region[index] != region[static_cast<std::size_t>(target)]) {
        fail(BytecodeError::Target, instruction.pc, static_cast<int>(index), "register target");
        return report;
      }
    }
  }
  return report;
}

std::string verify_bytecode(const BytecodeChunk& chunk) { return verify_bytecode_report(chunk).message(); }

}  // namespace clpp
