#include "clpp/clir.hpp"
#include "clpp/register_ir.hpp"
#include "clpp/bytecode_verifier.hpp"
#include "clpp/stack_verifier.hpp"

#include "core/compiler/bytecode_format.hpp"

#include <algorithm>
#include <vector>

namespace clpp {

namespace {

using Insn = compiler::bytecode::Instruction;

[[nodiscard]] bool writes_a(const RegOp op) {
  switch (op) {
    case RegOp::LoadK:
    case RegOp::Add:
    case RegOp::Sub:
    case RegOp::Mul:
    case RegOp::Div:
    case RegOp::Mod:
    case RegOp::Concat:
    case RegOp::Not:
    case RegOp::Eq:
    case RegOp::NotEq:
    case RegOp::Less:
    case RegOp::LessEq:
    case RegOp::Greater:
    case RegOp::GreaterEq:
    case RegOp::GetField:
    case RegOp::GetIndex:
    case RegOp::MakeBuffer:
    case RegOp::BufferSize:
    case RegOp::FileSize:
    case RegOp::Env:
    case RegOp::HttpHost:
    case RegOp::MakeTask:
    case RegOp::Await:
    case RegOp::Spawn:
      return true;
    default:
      return false;
  }
}

void fold_copies(std::vector<std::uint32_t>& code, const std::vector<char>& is_target) {
  for (std::size_t index = 0; index + 1 < code.size(); ++index) {
    const RegOp op = static_cast<RegOp>(reg_op(code[index]));
    const RegOp next = static_cast<RegOp>(reg_op(code[index + 1]));
    if (next != RegOp::Move || !writes_a(op) || is_target[index] != 0 || is_target[index + 1] != 0) {
      continue;
    }
    const std::uint8_t temp = reg_a(code[index]);
    const std::uint8_t dest = reg_a(code[index + 1]);
    if (reg_b(code[index + 1]) != temp || dest == temp) {
      continue;
    }
    bool used = false;
    for (std::size_t later = index + 2; later < code.size(); ++later) {
      const std::uint32_t word = code[later];
      const RegOp later_op = static_cast<RegOp>(reg_op(word));
      if (reg_a(word) == temp || reg_b(word) == temp || reg_c(word) == temp) {
        used = true;
        break;
      }
      if (writes_a(later_op) && reg_a(word) == temp) {
        break;
      }
    }
    if (used) {
      continue;
    }
    code[index] = (code[index] & ~0xFF00u) | (static_cast<std::uint32_t>(dest) << 8);
    code[index + 1] = reg_abc(RegOp::Move, dest, dest, 0);
  }
}

}  // namespace

RegChunk lower_registers(const BytecodeChunk& chunk) {
  RegChunk lowered;
  lowered.function_entry.assign(chunk.functions.size(), 0);
  lowered.error = verify_bytecode(chunk);
  if (!lowered.error.empty()) {
    return lowered;
  }
  if (chunk.code.empty()) {
    return lowered;
  }

  compiler::bytecode::DecodedChunk decoded;
  if (!compiler::bytecode::decode(chunk.code, decoded)) {
    lowered.error = "register decode";
    return lowered;
  }
  const std::vector<Insn>& insns = decoded.instructions;
  const std::vector<int>& at = decoded.instruction_at;
  const StackCheck stack = verify_stack(chunk, decoded);
  if (!stack.error.empty()) {
    lowered.error = stack.error;
    return lowered;
  }
  const std::vector<int>& depth = stack.depth;
  std::vector<int> roots(chunk.code.size(), -1);
  for (const FunctionBytecode& function : chunk.functions) {
    if (function.entry < roots.size()) {
      roots[function.entry] = std::max(roots[function.entry], static_cast<int>(function.local_count));
    }
  }
  if (chunk.entry < roots.size()) {
    roots[chunk.entry] = std::max(roots[chunk.entry], static_cast<int>(chunk.local_count));
  }

  std::vector<char> is_target(insns.size(), 0);
  for (const Insn& insn : insns) {
    if (insn.op == compiler::Opcode::Jump || insn.op == compiler::Opcode::JumpIfFalse ||
        insn.op == compiler::Opcode::Protect) {
      const int landed = compiler::bytecode::target_of(at, insn.imm);
      if (landed >= 0) {
        is_target[static_cast<std::size_t>(landed)] = 1;
      }
    }
  }
  for (std::size_t byte = 0; byte < roots.size(); ++byte) {
    const int landed = byte < at.size() ? at[byte] : -1;
    if (roots[byte] >= 0 && landed >= 0) {
      is_target[static_cast<std::size_t>(landed)] = 1;
    }
  }

  const auto fit = [&](const int reg) {
    if (reg < 0 || reg > 255) {
      lowered.error = "too many registers";
      return false;
    }
    return true;
  };

  std::vector<int> alias(256, -1);
  const auto clear_alias = [&]() { std::fill(alias.begin(), alias.end(), -1); };
    const auto resolve = [&](int reg) {
    int guard = 0;
    while (reg >= 0 && reg < 256 && alias[static_cast<std::size_t>(reg)] >= 0 && guard < 8) {
      reg = alias[static_cast<std::size_t>(reg)];
      ++guard;
    }
    return reg;
  };
  const auto kill = [&](const int reg) {
    if (reg >= 0 && reg < 256) {
      alias[static_cast<std::size_t>(reg)] = -1;
    }
    for (int& entry : alias) {
      if (entry == reg) {
        entry = -1;
      }
    }
  };

  for (std::size_t index = 0; index < insns.size(); ++index) {
    if (depth[index] < 0) {
      lowered.code.push_back(reg_abc(RegOp::Halt, 0, 0, 0));
      continue;
    }
    if (is_target[index] != 0) {
      clear_alias();
    }
    const Insn& insn = insns[index];
    const int sp = depth[index];
    const auto push_abc = [&](const RegOp op, const int a, const int b, const int c) {
      if (!fit(a) || !fit(b) || !fit(c)) {
        return false;
      }
      lowered.code.push_back(reg_abc(op, static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b),
                                     static_cast<std::uint8_t>(c)));
      return true;
    };
    const auto push_ad = [&](const RegOp op, const int a, const std::uint16_t d) {
      if (!fit(a)) {
        return false;
      }
      lowered.code.push_back(reg_ad(op, static_cast<std::uint8_t>(a), d));
      return true;
    };
    bool emitted = true;
    switch (insn.op) {
      case compiler::Opcode::Nop:
        emitted = push_abc(RegOp::Nop, 0, 0, 0);
        break;
      case compiler::Opcode::Halt:
        emitted = push_abc(RegOp::Halt, 0, 0, 0);
        break;
      case compiler::Opcode::Const:
        emitted = push_ad(RegOp::LoadK, sp, insn.imm);
        alias[static_cast<std::size_t>(sp)] = -1;
        break;
      case compiler::Opcode::LoadLocal:
        emitted = push_abc(RegOp::Move, sp, resolve(insn.imm), 0);
        if (emitted && sp < 256) {
          alias[static_cast<std::size_t>(sp)] = resolve(insn.imm);
        }
        break;
      case compiler::Opcode::StoreLocal:
        emitted = push_abc(RegOp::Move, insn.imm, resolve(sp - 1), 0);
        if (insn.imm < 256) {
          alias[insn.imm] = -1;
        }
        break;
      case compiler::Opcode::Dup:
        emitted = push_abc(RegOp::Move, sp, resolve(sp - 1), 0);
        if (emitted && sp < 256) {
          alias[static_cast<std::size_t>(sp)] = resolve(sp - 1);
        }
        break;
      case compiler::Opcode::Over:
        emitted = push_abc(RegOp::Move, sp, resolve(sp - 2), 0);
        if (emitted && sp < 256) {
          alias[static_cast<std::size_t>(sp)] = resolve(sp - 2);
        }
        break;
      case compiler::Opcode::SetField:
        // The object register holds its own copy (LoadLocal/Dup/GetField always move a value
        // into it), so it is changed in place; its alias is dropped so later reads see the change.
        emitted = push_abc(RegOp::SetField, sp - 2, resolve(sp - 1), insn.extra);
        kill(sp - 2);
        kill(sp - 1);
        break;
      case compiler::Opcode::SetIndex:
        emitted = push_abc(RegOp::SetIndex, sp - 3, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 3);
        kill(sp - 2);
        kill(sp - 1);
        break;
      case compiler::Opcode::MarkSelf:
        emitted = push_abc(RegOp::MarkSelf, 0, 0, 0);
        break;
      case compiler::Opcode::SelfBack:
        emitted = push_abc(RegOp::SelfBack, insn.imm, 0, 0);
        kill(insn.imm);
        break;
      // The list lives in the variable's own register and is changed in place.
      case compiler::Opcode::ListPush:
        emitted = push_abc(RegOp::ListPush, insn.imm, resolve(sp - 1), 0);
        kill(insn.imm);
        break;
      case compiler::Opcode::ListPop:
        emitted = push_abc(RegOp::ListPop, insn.imm, sp, 0);
        kill(insn.imm);
        kill(sp);
        break;
      case compiler::Opcode::ListInsert:
        emitted = push_abc(RegOp::ListInsert, insn.imm, resolve(sp - 2), resolve(sp - 1));
        kill(insn.imm);
        break;
      case compiler::Opcode::ListRemove:
        // the index register (a stack temporary) receives the removed element
        emitted = push_abc(RegOp::ListRemove, insn.imm, sp - 1, 0);
        kill(insn.imm);
        kill(sp - 1);
        break;
      case compiler::Opcode::Pop:
        emitted = push_abc(RegOp::Nop, 0, 0, 0);
        break;
      case compiler::Opcode::BitAnd:
        emitted = push_abc(RegOp::BitAnd, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::BitOr:
        emitted = push_abc(RegOp::BitOr, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::BitXor:
        emitted = push_abc(RegOp::BitXor, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::Shl:
        emitted = push_abc(RegOp::Shl, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::Shr:
        emitted = push_abc(RegOp::Shr, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::Range:
        emitted = push_abc(RegOp::Range, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::BitNot:
        emitted = push_abc(RegOp::BitNot, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Slice:
        emitted = push_abc(RegOp::Slice, sp - 3, sp - 3, 3);
        kill(sp - 3);
        break;
      case compiler::Opcode::PushError:
        emitted = push_abc(RegOp::PushError, sp, 0, 0);
        kill(sp);
        break;
      case compiler::Opcode::Add:
        emitted = push_abc(RegOp::Add, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::IntAdd:
        emitted = push_abc(RegOp::IntAdd, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::IntDiv:
        emitted = push_abc(RegOp::IntDiv, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Sub:
        emitted = push_abc(RegOp::Sub, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Mul:
        emitted = push_abc(RegOp::Mul, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Div:
        emitted = push_abc(RegOp::Div, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Mod:
        emitted = push_abc(RegOp::Mod, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Concat:
        emitted = push_abc(RegOp::Concat, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Not:
        emitted = push_abc(RegOp::Not, sp - 1, resolve(sp - 1), 0);
        if (sp >= 1 && sp - 1 < 256) {
          alias[static_cast<std::size_t>(sp - 1)] = -1;
        }
        break;
      case compiler::Opcode::Eq:
        emitted = push_abc(RegOp::Eq, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::NotEq:
        emitted = push_abc(RegOp::NotEq, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Less:
        emitted = push_abc(RegOp::Less, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::LessEq:
        emitted = push_abc(RegOp::LessEq, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Greater:
        emitted = push_abc(RegOp::Greater, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::GreaterEq:
        emitted = push_abc(RegOp::GreaterEq, sp - 2, resolve(sp - 2), resolve(sp - 1));
        if (sp >= 2 && sp - 2 < 256) {
          alias[static_cast<std::size_t>(sp - 2)] = -1;
        }
        break;
      case compiler::Opcode::Jump:
        emitted = push_ad(RegOp::Jump, 0,
              static_cast<std::uint16_t>(compiler::bytecode::target_of(at, insn.imm)));
        clear_alias();
        break;
      case compiler::Opcode::JumpIfFalse:
        emitted = push_ad(RegOp::JumpIfFalse, resolve(sp - 1),
              static_cast<std::uint16_t>(compiler::bytecode::target_of(at, insn.imm)));
        clear_alias();
        break;
      case compiler::Opcode::Post:
        emitted = push_abc(RegOp::Post, resolve(sp - 1), 0, 0);
        break;
      case compiler::Opcode::Warn:
        emitted = push_abc(RegOp::Warn, resolve(sp - 1), 0, 0);
        break;
      case compiler::Opcode::Report:
        emitted = push_abc(RegOp::Report, resolve(sp - 1), 0, 0);
        break;
      case compiler::Opcode::Call:
      case compiler::Opcode::Schedule:
      case compiler::Opcode::SpawnThread: {
        const int arity = static_cast<int>(chunk.functions[insn.imm].arity);
        RegOp op = RegOp::Call;
        if (insn.op == compiler::Opcode::SpawnThread) {
          op = RegOp::SpawnThread;
        } else if (insn.op == compiler::Opcode::Schedule) {
          op = RegOp::Schedule;
        }
        emitted = push_ad(op, sp - arity, insn.imm);
        clear_alias();
        break;
      }
      case compiler::Opcode::VCall:
        emitted = push_ad(RegOp::VCall, sp - static_cast<int>(insn.aux), insn.imm);
        clear_alias();
        break;
      case compiler::Opcode::Tag:
        emitted = push_ad(RegOp::Tag, resolve(sp - 1), insn.imm);
        kill(sp - 1);
        break;
      case compiler::Opcode::CoCreate:
        emitted = push_ad(RegOp::CoCreate, sp, insn.imm);
        kill(sp);
        break;
      case compiler::Opcode::Defer:
        emitted = push_ad(RegOp::Defer, sp, insn.imm);
        kill(sp);
        break;
      case compiler::Opcode::CoResume:
        emitted = push_abc(RegOp::CoResume, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::CoYield:
        emitted = push_abc(RegOp::CoYield, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Actor:
        emitted = push_ad(RegOp::Actor, resolve(sp - 1), insn.imm);
        kill(sp - 1);
        break;
      case compiler::Opcode::CCall:
        emitted = push_ad(RegOp::CCall, sp - 1, insn.imm);
        kill(sp - 1);
        break;
      case compiler::Opcode::ListSort:
        emitted = push_abc(RegOp::ListSort, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::ListFind:
        emitted = push_abc(RegOp::ListFind, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::IterAt:
        emitted = push_abc(RegOp::IterAt, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::ListLen:
        emitted = push_abc(RegOp::ListLen, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::IterLen:
        emitted = push_abc(RegOp::IterLen, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Join:
        emitted = push_abc(RegOp::Join, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::AtomicNew:
        emitted = push_abc(RegOp::AtomicNew, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::FetchAdd:
        emitted = push_abc(RegOp::FetchAdd, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::AtomicLoad:
        emitted = push_abc(RegOp::AtomicLoad, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::MutexNew:
        emitted = push_abc(RegOp::MutexNew, sp, 0, 0);
        kill(sp);
        break;
      case compiler::Opcode::Lock:
        emitted = push_abc(RegOp::Lock, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Unlock:
        emitted = push_abc(RegOp::Unlock, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Return:
        emitted = push_abc(RegOp::Return, resolve(sp - 1), 0, 0);
        clear_alias();
        break;
      case compiler::Opcode::MakeVector:
        emitted = push_abc(RegOp::MakeVector, sp - insn.extra, sp - insn.extra, insn.extra);
        kill(sp - insn.extra);
        break;
      case compiler::Opcode::Axiom:
      case compiler::Opcode::Std:
        emitted = push_abc(insn.op == compiler::Opcode::Std ? RegOp::Std : RegOp::Axiom,
                           sp - static_cast<int>(insn.imm >> 8), static_cast<int>(insn.imm >> 8),
                           static_cast<int>(insn.imm & 0xFF));
        kill(sp - static_cast<int>(insn.imm >> 8));
        break;
      case compiler::Opcode::MakeBuffer:
        emitted = push_abc(RegOp::MakeBuffer, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::BufferWrite:
        emitted = push_abc(RegOp::BufferWrite, sp - 3, resolve(sp - 3), 0);
        kill(sp - 3);
        break;
      case compiler::Opcode::BufferSize:
        emitted = push_abc(RegOp::BufferSize, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::GetField:
        emitted = push_abc(RegOp::GetField, sp - 1, resolve(sp - 1), insn.extra);
        kill(sp - 1);
        break;
      case compiler::Opcode::MakeStruct:
        emitted = push_ad(RegOp::MakeStruct, sp - static_cast<int>(insn.imm), insn.imm);
        kill(sp - static_cast<int>(insn.imm));
        break;
      case compiler::Opcode::GetSelf:
        emitted = push_ad(RegOp::GetSelf, sp, insn.imm);
        kill(sp);
        break;
      case compiler::Opcode::SetSelf:
        emitted = push_ad(RegOp::SetSelf, resolve(sp - 1), insn.imm);
        break;
      case compiler::Opcode::MakeTask:
        emitted = push_abc(RegOp::MakeTask, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Await:
        emitted = push_abc(RegOp::Await, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Spawn:
        emitted = push_abc(RegOp::Spawn, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Parallel:
        emitted = push_ad(RegOp::Parallel, sp - static_cast<int>(insn.imm), insn.imm);
        kill(sp - static_cast<int>(insn.imm));
        break;
      case compiler::Opcode::FileSize:
        emitted = push_abc(RegOp::FileSize, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::Env:
        emitted = push_abc(RegOp::Env, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::HttpHost:
        emitted = push_abc(RegOp::HttpHost, sp - 1, resolve(sp - 1), 0);
        kill(sp - 1);
        break;
      case compiler::Opcode::GetIndex:
        emitted = push_abc(RegOp::GetIndex, sp - 2, resolve(sp - 2), resolve(sp - 1));
        kill(sp - 2);
        break;
      case compiler::Opcode::ClearLocal:
        emitted = push_abc(RegOp::Clear, insn.imm, 0, 0);
        kill(insn.imm);
        break;
      case compiler::Opcode::Protect:
        emitted = push_ad(RegOp::Protect, 0,
              static_cast<std::uint16_t>(compiler::bytecode::target_of(at, insn.imm)));
        break;
      case compiler::Opcode::EndTry:
        emitted = push_abc(RegOp::EndTry, 0, 0, 0);
        break;
      default:
        lowered.error = "register decode";
        return lowered;
    }
    if (!lowered.error.empty() || !emitted) {
      if (lowered.error.empty()) {
        lowered.error = "too many registers";
      }
      return lowered;
    }
  }

  fold_copies(lowered.code, is_target);
  lowered.origin.reserve(insns.size());
  for (const Insn& insn : insns) {
    lowered.origin.push_back(static_cast<std::uint16_t>(insn.pc));
  }

  for (std::size_t index = 0; index < chunk.functions.size(); ++index) {
    const int mapped = compiler::bytecode::target_of(at, chunk.functions[index].entry);
    lowered.function_entry[index] = mapped < 0 ? 0 : static_cast<std::uint16_t>(mapped);
  }
  const int main_entry = compiler::bytecode::target_of(at, chunk.entry);
  lowered.entry = main_entry < 0 ? 0 : static_cast<std::uint16_t>(main_entry);

  // Frame size per function: the highest register named by any instruction of its region
  // (a region runs from its entry to the next entry, as the verifier already requires).
  std::vector<std::size_t> starts;
  for (const std::uint16_t entry : lowered.function_entry) {
    starts.push_back(entry);
  }
  starts.push_back(lowered.entry);
  std::sort(starts.begin(), starts.end());
  starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
  lowered.function_frame.assign(chunk.functions.size(), 256);
  for (std::size_t index = 0; index < chunk.functions.size(); ++index) {
    const std::size_t begin = lowered.function_entry[index];
    const auto next = std::upper_bound(starts.begin(), starts.end(), begin);
    const std::size_t end = next == starts.end() ? lowered.code.size() : *next;
    int highest = static_cast<int>(chunk.functions[index].local_count) + static_cast<int>(chunk.functions[index].arity);
    for (std::size_t pc = begin; pc < end && pc < lowered.code.size(); ++pc) {
      const std::uint32_t word = lowered.code[pc];
      highest = std::max({highest, static_cast<int>(reg_a(word)), static_cast<int>(reg_b(word)),
                          static_cast<int>(reg_c(word))});
    }
    // Headroom for instructions that address a run of registers starting at A (calls, vectors).
    lowered.function_frame[index] = static_cast<std::uint16_t>(std::min(256, highest + 1 + 16));
  }
  return lowered;
}

RegChunk lower_to_vm(const BytecodeChunk& chunk) { return lower_registers(chunk); }

}  // namespace clpp
