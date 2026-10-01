#include "clpp/clir.hpp"

#include <cstdint>
#include <string>

namespace clpp {

namespace {

void append_u16(std::string& out, const std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>(value >> 8));
}

void append_u32(std::string& out, const std::uint32_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>(value >> 24));
}

[[nodiscard]] bool read_u16(std::string_view& bytes, std::uint16_t& value) {
  if (bytes.size() < 2) {
    return false;
  }
  value = static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0]) |
                                     (static_cast<unsigned char>(bytes[1]) << 8));
  bytes.remove_prefix(2);
  return true;
}

[[nodiscard]] bool read_u32(std::string_view& bytes, std::uint32_t& value) {
  if (bytes.size() < 4) {
    return false;
  }
  value = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
  bytes.remove_prefix(4);
  return true;
}

}  // namespace

std::string serialize_clir(const BytecodeChunk& chunk) {
  std::string out = "CLIR";
  append_u16(out, chunk.version);
  append_u16(out, chunk.entry);
  append_u16(out, chunk.local_count);
  append_u32(out, static_cast<std::uint32_t>(chunk.code.size()));
  out.append(reinterpret_cast<const char*>(chunk.code.data()), chunk.code.size());
  append_u16(out, static_cast<std::uint16_t>(chunk.functions.size()));
  for (const FunctionBytecode& function : chunk.functions) {
    append_u16(out, function.arity);
    append_u16(out, function.local_count);
    append_u16(out, function.entry);
  }
  return out;
}

BytecodeChunk deserialize_clir(const std::string_view bytes_in) {
  BytecodeChunk chunk;
  std::string_view bytes = bytes_in;
  if (bytes.size() < 4 || bytes.substr(0, 4) != "CLIR") {
    chunk.version = 0;
    return chunk;
  }
  bytes.remove_prefix(4);
  std::uint16_t version = 0;
  std::uint32_t code_size = 0;
  std::uint16_t functions = 0;
  if (!read_u16(bytes, version) || !read_u16(bytes, chunk.entry) || !read_u16(bytes, chunk.local_count) ||
      !read_u32(bytes, code_size) || bytes.size() < code_size) {
    chunk.version = 0;
    return chunk;
  }
  chunk.version = version;
  chunk.code.assign(code_size, 0);
  for (std::uint32_t index = 0; index < code_size; ++index) {
    chunk.code[index] = static_cast<std::uint8_t>(bytes[index]);
  }
  bytes.remove_prefix(code_size);
  if (!read_u16(bytes, functions)) {
    chunk.version = 0;
    chunk.code.clear();
    return chunk;
  }
  chunk.functions.resize(functions);
  for (std::uint16_t index = 0; index < functions; ++index) {
    if (!read_u16(bytes, chunk.functions[index].arity) || !read_u16(bytes, chunk.functions[index].local_count) ||
        !read_u16(bytes, chunk.functions[index].entry)) {
      chunk.version = 0;
      chunk.code.clear();
      chunk.functions.clear();
      return chunk;
    }
  }
  return chunk;
}

}  // namespace clpp
