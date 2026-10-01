#include "clpp/clir.hpp"

#include "core/compiler/bytecode_format.hpp"
#include "core/compiler/opcode.hpp"

namespace clpp {

namespace {

[[nodiscard]] bool luau_supports(const compiler::Opcode op) {
  switch (op) {
    case compiler::Opcode::FileSize:
    case compiler::Opcode::Env:
    case compiler::Opcode::HttpHost:
    case compiler::Opcode::Actor:
    case compiler::Opcode::AtomicNew:
    case compiler::Opcode::FetchAdd:
    case compiler::Opcode::AtomicLoad:
    case compiler::Opcode::MutexNew:
    case compiler::Opcode::Lock:
    case compiler::Opcode::Unlock:
    case compiler::Opcode::SpawnThread:
    case compiler::Opcode::Join:
      return false;
    default:
      return true;
  }
}

}  // namespace

std::string check_backend(const Backend backend, const BytecodeChunk& chunk) {
  if (backend == Backend::Vm) {
    return {};
  }
  compiler::bytecode::DecodedChunk decoded;
  if (!compiler::bytecode::decode(chunk.code, decoded)) {
    return "luau backend: chunk does not decode";
  }
  for (const compiler::bytecode::Instruction& instruction : decoded.instructions) {
    if (!luau_supports(instruction.op)) {
      return "luau backend: unsupported host opcode";
    }
  }
  return {};
}

}  // namespace clpp
