#include "clpp/clir.hpp"

#include "clpp/bytecode_verifier.hpp"
#include "clpp/stack_verifier.hpp"
#include "core/compiler/bytecode_format.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace clpp {
namespace {

static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

enum class LiteralTag : std::uint8_t { Number, String, Vector, Null, Integer, Boolean };

void append_u16(std::string& out, const std::uint16_t value) {
  for (unsigned shift = 0; shift < 16; shift += 8) out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

void append_u32(std::string& out, const std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

void append_integer(std::string& out, const std::int64_t value) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<char>((bits >> shift) & 0xFF));
}

void append_double(std::string& out, const double value) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<char>((bits >> shift) & 0xFF));
}

[[nodiscard]] std::uint16_t count16(const std::size_t count) {
  if (count > std::numeric_limits<std::uint16_t>::max()) throw std::invalid_argument("CLIR table exceeds 16-bit limit");
  return static_cast<std::uint16_t>(count);
}

[[nodiscard]] std::uint32_t count32(const std::size_t count) {
  if (count > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("CLIR payload exceeds 32-bit limit");
  return static_cast<std::uint32_t>(count);
}

class Reader {
 public:
  explicit Reader(const std::string_view bytes) : remaining(bytes) {}

  bool u8(std::uint8_t& value) {
    if (remaining.empty()) return false;
    value = static_cast<std::uint8_t>(remaining.front());
    remaining.remove_prefix(1);
    return true;
  }

  bool u16(std::uint16_t& value) {
    if (remaining.size() < 2) return false;
    value = 0;
    for (unsigned i = 0; i < 2; ++i) value |= static_cast<std::uint16_t>(static_cast<unsigned char>(remaining[i]) << (8 * i));
    remaining.remove_prefix(2);
    return true;
  }

  bool u32(std::uint32_t& value) {
    if (remaining.size() < 4) return false;
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(static_cast<unsigned char>(remaining[i])) << (8 * i);
    remaining.remove_prefix(4);
    return true;
  }

  bool integer(std::int64_t& value) {
    if (remaining.size() < 8) return false;
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(remaining[i])) << (8 * i);
    value = std::bit_cast<std::int64_t>(bits);
    remaining.remove_prefix(8);
    return true;
  }

  bool number(double& value) {
    if (remaining.size() < 8) return false;
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(remaining[i])) << (8 * i);
    value = std::bit_cast<double>(bits);
    remaining.remove_prefix(8);
    return true;
  }

  bool text(std::string& value) {
    std::uint32_t size = 0;
    if (!u32(size) || size > remaining.size()) return false;
    value.assign(remaining.substr(0, size));
    remaining.remove_prefix(size);
    return true;
  }

  std::string_view remaining;
};

void append_literal(std::string& out, const Value& value) {
  switch (value.kind) {
    case Value::Kind::Boolean:
      if (value.number != 0 && value.number != 1) throw std::invalid_argument("invalid CLIR boolean value");
      out.push_back(static_cast<char>(LiteralTag::Boolean));
      out.push_back(static_cast<char>(value.number != 0));
      break;
    case Value::Kind::Integer:
      out.push_back(static_cast<char>(LiteralTag::Integer));
      append_integer(out, value.integer);
      break;
    case Value::Kind::Number:
      out.push_back(static_cast<char>(LiteralTag::Number));
      append_double(out, value.number);
      break;
    case Value::Kind::String:
      out.push_back(static_cast<char>(LiteralTag::String));
      append_u32(out, count32(value.text.size()));
      out.append(value.text);
      break;
    case Value::Kind::Vector:
      if (value.dims < 2 || value.dims > 4) throw std::invalid_argument("invalid CLIR vector dimensions");
      out.push_back(static_cast<char>(LiteralTag::Vector));
      out.push_back(static_cast<char>(value.dims));
      append_double(out, value.number);
      append_double(out, value.y);
      append_double(out, value.z);
      append_double(out, value.w);
      break;
    case Value::Kind::Null:
      out.push_back(static_cast<char>(LiteralTag::Null));
      break;
    default:
      throw std::invalid_argument("runtime heap values cannot be serialized as CLIR literals");
  }
}

bool read_literal(Reader& reader, Value& value) {
  std::uint8_t tag = 0;
  if (!reader.u8(tag)) return false;
  switch (static_cast<LiteralTag>(tag)) {
    case LiteralTag::Boolean: {
      std::uint8_t boolean = 0;
      if (!reader.u8(boolean) || boolean > 1) return false;
      value = Value::boolean_of(boolean != 0); return true;
    }
    case LiteralTag::Integer: {
      std::int64_t integer = 0;
      if (!reader.integer(integer)) return false;
      value = Value::integer_of(integer);
      return true;
    }
    case LiteralTag::Number:
      value = Value::number_of(0);
      return reader.number(value.number);
    case LiteralTag::String:
      value = Value::string_of({});
      return reader.text(value.text);
    case LiteralTag::Vector:
      value = Value::vector_of(0, 0, 0);
      return reader.u8(value.dims) && value.dims >= 2 && value.dims <= 4 && reader.number(value.number) &&
             reader.number(value.y) && reader.number(value.z) && reader.number(value.w);
    case LiteralTag::Null:
      value = Value::null_of();
      return true;
    default:
      return false;
  }
}

BytecodeChunk invalid_chunk() {
  BytecodeChunk chunk;
  chunk.version = 0;
  return chunk;
}

bool executable_valid(const BytecodeChunk& chunk) {
  if (!verify_bytecode(chunk).empty()) return false;
  compiler::bytecode::DecodedChunk decoded;
  return compiler::bytecode::decode(chunk.code, decoded) && verify_stack(chunk, decoded).error.empty();
}

bool source_map_valid(const BytecodeChunk& chunk) {
  for (const auto& file : chunk.source_files) if (file.find('\0') != std::string::npos) return false;
  std::uint16_t previous = 0;
  bool first = true;
  for (const auto& entry : chunk.source_map) {
    if (entry.pc >= chunk.code.size() || entry.location.line == 0 || entry.location.column == 0 ||
        (entry.location.source != 0 && entry.location.source >= chunk.source_files.size()) ||
        (!first && entry.pc < previous)) return false;
    first = false;
    previous = entry.pc;
  }
  return true;
}

}  // namespace

std::string serialize_clir(const BytecodeChunk& chunk) {
  if (!executable_valid(chunk) || !source_map_valid(chunk)) throw std::invalid_argument("invalid CLIR chunk");
  std::string out = "CLIR";
  append_u16(out, kClirContainerVersion);
  append_u16(out, chunk.version);
  append_u16(out, chunk.entry);
  append_u16(out, chunk.local_count);
  append_u32(out, count32(chunk.code.size()));
  if (!chunk.code.empty()) out.append(reinterpret_cast<const char*>(chunk.code.data()), chunk.code.size());
  append_u16(out, count16(chunk.constants.size()));
  for (const Value& value : chunk.constants) append_literal(out, value);
  append_u16(out, count16(chunk.functions.size()));
  for (const auto& function : chunk.functions) {
    append_u16(out, function.arity);
    append_u16(out, function.local_count);
    append_u16(out, function.entry);
    append_u16(out, function.is_async ? 1 : 0);
  }
  append_u16(out, count16(chunk.vtables.size()));
  for (const auto& table : chunk.vtables) {
    append_u16(out, count16(table.size()));
    for (const auto function : table) append_u16(out, function);
  }
  append_u32(out, count32(chunk.source_map.size()));
  for (const auto& entry : chunk.source_map) {
    append_u16(out, entry.pc);
    append_u32(out, entry.location.line);
    append_u32(out, entry.location.column);
    append_u32(out, entry.location.source);
  }
  append_u32(out, count32(chunk.source_files.size()));
  for (const auto& file : chunk.source_files) {
    append_u32(out, count32(file.size()));
    out.append(file);
  }
  return out;
}

BytecodeChunk deserialize_clir(const std::string_view bytes) {
  if (bytes.size() < 4 || bytes.substr(0, 4) != "CLIR") return invalid_chunk();
  Reader reader(bytes.substr(4));
  BytecodeChunk chunk;
  std::uint16_t container = 0;
  std::uint32_t code_size = 0;
  if (!reader.u16(container) || container != kClirContainerVersion || !reader.u16(chunk.version) ||
      chunk.version != kBytecodeFormatVersion || !reader.u16(chunk.entry) || !reader.u16(chunk.local_count) ||
      !reader.u32(code_size) || code_size > std::numeric_limits<std::uint16_t>::max() ||
      code_size > reader.remaining.size()) return invalid_chunk();
  for (std::uint32_t i = 0; i < code_size; ++i) chunk.code.push_back(static_cast<std::uint8_t>(reader.remaining[i]));
  reader.remaining.remove_prefix(code_size);

  std::uint16_t count = 0;
  if (!reader.u16(count) || count > reader.remaining.size()) return invalid_chunk();
  for (std::uint16_t i = 0; i < count; ++i) {
    Value literal;
    if (!read_literal(reader, literal)) return invalid_chunk();
    chunk.constants.push_back(std::move(literal));
  }
  if (!reader.u16(count) || count > reader.remaining.size() / 8) return invalid_chunk();
  for (std::uint16_t i = 0; i < count; ++i) {
    FunctionBytecode function;
    if (!reader.u16(function.arity) || !reader.u16(function.local_count) || !reader.u16(function.entry)) return invalid_chunk();
    std::uint16_t flags = 0;
    if (!reader.u16(flags) || flags > 1) return invalid_chunk();
    function.is_async = flags != 0;
    chunk.functions.push_back(function);
  }
  if (!reader.u16(count) || count > reader.remaining.size() / 2) return invalid_chunk();
  for (std::uint16_t i = 0; i < count; ++i) {
    std::uint16_t size = 0;
    if (!reader.u16(size) || size > reader.remaining.size() / 2) return invalid_chunk();
    std::vector<std::uint16_t> table;
    for (std::uint16_t j = 0; j < size; ++j) {
      std::uint16_t function = 0;
      if (!reader.u16(function)) return invalid_chunk();
      table.push_back(function);
    }
    chunk.vtables.push_back(std::move(table));
  }
  std::uint32_t source_count = 0;
  if (!reader.u32(source_count) || source_count > reader.remaining.size() / 14) return invalid_chunk();
  for (std::uint32_t i = 0; i < source_count; ++i) {
    SourceMapEntry entry;
    if (!reader.u16(entry.pc) || !reader.u32(entry.location.line) || !reader.u32(entry.location.column) ||
        !reader.u32(entry.location.source)) return invalid_chunk();
    chunk.source_map.push_back(entry);
  }
  std::uint32_t file_count = 0;
  if (!reader.u32(file_count) || file_count > reader.remaining.size() / 4) return invalid_chunk();
  for (std::uint32_t i = 0; i < file_count; ++i) {
    std::uint32_t size = 0;
    if (!reader.u32(size) || size > reader.remaining.size()) return invalid_chunk();
    chunk.source_files.emplace_back(reader.remaining.substr(0, size));
    reader.remaining.remove_prefix(size);
  }
  if (!reader.remaining.empty() || !source_map_valid(chunk) || !executable_valid(chunk)) return invalid_chunk();
  return chunk;
}

}  // namespace clpp
