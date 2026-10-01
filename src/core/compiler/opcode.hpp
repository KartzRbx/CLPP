#pragma once

#include <cstdint>

namespace clpp::compiler {

enum class Opcode : std::uint8_t {
  Nop = 0,
  Halt = 1,
  Const = 2,
  Concat = 3,
  Post = 4,
  Add = 5,
  Sub = 6,
  Mul = 7,
  Div = 8,
  Mod = 9,
  LoadLocal = 10,
  StoreLocal = 11,
  Jump = 12,
  JumpIfFalse = 13,
  Not = 14,
  Eq = 15,
  NotEq = 16,
  Less = 17,
  LessEq = 18,
  Greater = 19,
  GreaterEq = 20,
  Dup = 21,
  Pop = 22,
  Call = 23,
  Return = 24,
  MakeVector = 25,
  MakeBuffer = 26,
  BufferWrite = 27,
  BufferSize = 28,
  GetField = 29,
  MakeStruct = 30,
  GetSelf = 31,
  SetSelf = 32,
  MakeTask = 33,
  Await = 34,
  Spawn = 35,
  Parallel = 36,
  FileSize = 37,
  Env = 38,
  HttpHost = 39,
  Protect = 40,
  EndTry = 41,
  GetIndex = 42,
  ClearLocal = 43,
  SpawnThread = 44,
  Join = 45,
  AtomicNew = 46,
  FetchAdd = 47,
  AtomicLoad = 48,
  MutexNew = 49,
  Lock = 50,
  Unlock = 51,
  ListSort = 52,
  ListFind = 53,
  CCall = 54,
  IntAdd = 56,
  VCall = 57,
  Tag = 58,
  CoCreate = 59,
  CoResume = 60,
  CoYield = 61,
  Defer = 62,
  Actor = 63,
  Axiom = 64,
  Schedule = 65,
  BitAnd = 66,
  BitOr = 67,
  BitXor = 68,
  BitNot = 69,
  Shl = 70,
  Shr = 71,
  Range = 72,
  Slice = 73,
  PushError = 74,
  Std = 75,
  IntDiv = 76,  // both operands typed int: truncates toward zero, like C++
  Warn = 77,    // print to the error stream
  Report = 78,  // raise a runtime error with the value as message
  ListLen = 79,  // element count of a list/array/dictionary, character count of a string
  IterLen = 80,  // for-in limit: a number stays itself, a collection gives its length
  IterAt = 81,   // for-in item: a number gives the position, a collection its element
  SetField = 82,  // [object, value] -> [object with field `extra` replaced]
  SetIndex = 83,  // [object, index, value] -> [object with the element replaced]
  Over = 84,      // [a, b] -> [a, b, a]
  MarkSelf = 85,  // in a method that changes self: remember the final self for the caller
  SelfBack = 86,  // after a method call on a variable: write the changed self back into local `imm`
  ListPush = 87,    // local `imm` (a list) gets [value] appended
  ListPop = 88,     // [] -> [last element of local `imm`], removed
  ListInsert = 89,  // [index, value] -> [], inserted into local `imm`
  ListRemove = 90,  // [index or key] -> [removed element], from local `imm` (list or dictionary)
  Host = 91,        // like Axiom: imm = (arity << 8) | id into the host library registry (window, gfx, ui, json...)
};

}  // namespace clpp::compiler
