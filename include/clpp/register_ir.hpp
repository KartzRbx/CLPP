#pragma once

#include "clpp/compiler.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace clpp {

enum class RegOp : std::uint8_t {
  Nop = 0,
  Halt = 1,
  LoadK = 2,
  Move = 3,
  Add = 4,
  Sub = 5,
  Mul = 6,
  Div = 7,
  Mod = 8,
  Concat = 9,
  Not = 10,
  Eq = 11,
  NotEq = 12,
  Less = 13,
  LessEq = 14,
  Greater = 15,
  GreaterEq = 16,
  Jump = 17,
  JumpIfFalse = 18,
  Post = 19,
  Call = 20,
  Return = 21,
  MakeVector = 22,
  MakeBuffer = 23,
  BufferWrite = 24,
  BufferSize = 25,
  GetField = 26,
  MakeStruct = 27,
  GetSelf = 28,
  SetSelf = 29,
  MakeTask = 30,
  Await = 31,
  Spawn = 32,
  Parallel = 33,
  FileSize = 34,
  Env = 35,
  HttpHost = 36,
  Protect = 37,
  EndTry = 38,
  GetIndex = 39,
  Clear = 40,
  SpawnThread = 41,
  Join = 42,
  AtomicNew = 43,
  FetchAdd = 44,
  AtomicLoad = 45,
  MutexNew = 46,
  Lock = 47,
  Unlock = 48,
  ListSort = 49,
  ListFind = 50,
  CCall = 51,
  IntAdd = 53,
  VCall = 54,
  Tag = 55,
  CoCreate = 56,
  CoResume = 57,
  CoYield = 58,
  Defer = 59,
  Actor = 60,
  Axiom = 61,
  Schedule = 62,
  BitAnd = 63,
  BitOr = 64,
  BitXor = 65,
  BitNot = 66,
  Shl = 67,
  Shr = 68,
  Range = 69,
  Slice = 70,
  PushError = 71,
  Std = 72,
  IntDiv = 73,
  Warn = 74,
  Report = 75,
  ListLen = 76,
  IterLen = 77,
  IterAt = 78,
  SetField = 79,
  SetIndex = 80,
  MarkSelf = 81,
  SelfBack = 82,
  ListPush = 83,
  ListPop = 84,
  ListInsert = 85,
  ListRemove = 86,
};

[[nodiscard]] inline std::uint32_t reg_abc(const RegOp op, const std::uint8_t a, const std::uint8_t b, const std::uint8_t c) {
  return static_cast<std::uint8_t>(op) | (static_cast<std::uint32_t>(a) << 8) | (static_cast<std::uint32_t>(b) << 16) |
         (static_cast<std::uint32_t>(c) << 24);
}

[[nodiscard]] inline std::uint32_t reg_ad(const RegOp op, const std::uint8_t a, const std::uint16_t d) {
  return static_cast<std::uint8_t>(op) | (static_cast<std::uint32_t>(a) << 8) | (static_cast<std::uint32_t>(d) << 16);
}

[[nodiscard]] inline std::uint8_t reg_op(const std::uint32_t word) { return static_cast<std::uint8_t>(word & 0xFF); }

[[nodiscard]] inline std::uint8_t reg_a(const std::uint32_t word) { return static_cast<std::uint8_t>((word >> 8) & 0xFF); }

[[nodiscard]] inline std::uint8_t reg_b(const std::uint32_t word) { return static_cast<std::uint8_t>((word >> 16) & 0xFF); }

[[nodiscard]] inline std::uint8_t reg_c(const std::uint32_t word) { return static_cast<std::uint8_t>((word >> 24) & 0xFF); }

[[nodiscard]] inline std::uint16_t reg_d(const std::uint32_t word) {
  return static_cast<std::uint16_t>((word >> 16) & 0xFFFF);
}

struct RegChunk {
  std::vector<std::uint32_t> code;
  std::vector<std::uint16_t> function_entry;
  // Registers each function really touches (upper bound, <= 256). The VM sizes call frames with
  // it instead of always allocating 256 registers per call.
  std::vector<std::uint16_t> function_frame;
  // Stack-bytecode offset each register instruction came from (for error locations).
  std::vector<std::uint16_t> origin;
  std::uint16_t entry{0};
  std::string error;
};

// Lowers stack bytecode into 32-bit ABC/AD instructions.
// ADD A B C means R[A] = R[B] + R[C].
[[nodiscard]] RegChunk lower_registers(const BytecodeChunk& chunk);

}  // namespace clpp
