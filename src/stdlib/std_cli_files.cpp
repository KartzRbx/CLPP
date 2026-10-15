// General CLI file operations. No compiler, project or target-language protocol.
#include "stdlib/host.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace clpp::stdlib::host {
namespace {
constexpr std::size_t kTextLimit = 64 * 1024 * 1024;
constexpr std::size_t kByteLimit = 4 * 1024 * 1024;

std::filesystem::path utf8_path(const std::string& text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
bool path_argument(const Value& value, std::filesystem::path& path, std::string& error) {
  if (!value.is_string() || value.text.empty() || value.text.find('\0') != std::string::npos ||
      value.text.find("..") != std::string::npos) {
    return fail(error, "Fs: expected a nonempty path without NUL or '..'");
  }
  path = utf8_path(value.text);
  return true;
}

bool read_file(const Value* args, const std::uint8_t arity, Value& out, std::string& error, const bool bytes) {
  try {
    std::filesystem::path path;
    const auto budget = bytes ? kByteLimit : kTextLimit;
    if (arity != 2 || !path_argument(args[0], path, error)) return false;
    if (!args[1].is_number() || !std::isfinite(args[1].number) || std::trunc(args[1].number) != args[1].number ||
        args[1].number < 1 || args[1].number > static_cast<double>(budget)) {
      return fail(error, "Fs: invalid read byte limit");
    }
    if (!std::filesystem::is_regular_file(path)) return fail(error, "Fs: expected a regular file");
    std::ifstream file(path, std::ios::binary);
    if (!file) return fail(error, "Fs: cannot open file for reading");
    const auto maximum = static_cast<std::size_t>(args[1].number);
    std::string data;
    char block[8192];
    while (file) {
      file.read(block, static_cast<std::streamsize>(sizeof(block)));
      const auto count = static_cast<std::size_t>(file.gcount());
      if (count > maximum - data.size()) return fail(error, "Fs: file exceeds read byte limit");
      data.append(block, count);
    }
    if (!file.eof()) return fail(error, "Fs: file read failed");
    if (!bytes) { out = Value::string_of(std::move(data)); return true; }
    std::vector<Value> values;
    values.reserve(data.size());
    for (const unsigned char byte : data) values.push_back(Value::integer_of(byte));
    out = Value::struct_of(std::move(values));
    return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Fs: ") + failure.what()); }
}
bool read_text(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  return read_file(args, arity, out, error, false);
}
bool read_bytes(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  return read_file(args, arity, out, error, true);
}

// An exclusive temporary file lives beside the destination, so replacement cannot
// cross filesystem boundaries. Only a completely written, flushed file is renamed.
bool replace_file(const std::filesystem::path& destination, const std::string& data, std::string& error) {
#ifdef _WIN32
  std::random_device entropy;
  static std::atomic<unsigned long> sequence{0};
  std::filesystem::path temporary;
  HANDLE file = INVALID_HANDLE_VALUE;
  for (unsigned int attempt = 0; attempt < 32 && file == INVALID_HANDLE_VALUE; ++attempt) {
    temporary = destination;
    temporary += L".clpp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(entropy()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
      return fail(error, "Fs.WriteAtomic: cannot create temporary file");
  }
  if (file == INVALID_HANDLE_VALUE) return fail(error, "Fs.WriteAtomic: cannot reserve temporary file");
  struct Cleanup {
    HANDLE& file;
    const std::filesystem::path& path;
    bool committed{false};
    ~Cleanup() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); if (!committed) DeleteFileW(path.c_str()); }
  } cleanup{file, temporary};
  for (std::size_t offset = 0; offset < data.size();) {
    DWORD written = 0;
    const auto count = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 65536));
    if (!WriteFile(file, data.data() + offset, count, &written, nullptr) || written == 0)
      return fail(error, "Fs.WriteAtomic: file write failed");
    offset += written;
  }
  if (!FlushFileBuffers(file)) return fail(error, "Fs.WriteAtomic: file flush failed");
  const bool closed = CloseHandle(file) != 0;
  file = INVALID_HANDLE_VALUE;
  if (!closed) return fail(error, "Fs.WriteAtomic: file close failed");
  bool replaced = false;
  DWORD replacement_error = 0;
  for (unsigned int attempt = 0; attempt < 40; ++attempt) {
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      replaced = true; break;
    }
    replacement_error = GetLastError();
    if (replacement_error != ERROR_ACCESS_DENIED && replacement_error != ERROR_SHARING_VIOLATION) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!replaced) return fail(error, "Fs.WriteAtomic: destination replacement failed (Windows error " + std::to_string(replacement_error) + ")");
  cleanup.committed = true;
#else
  std::string pattern = destination.string() + ".clpp-XXXXXX";
  int descriptor = mkstemp(pattern.data());
  if (descriptor < 0) return fail(error, "Fs.WriteAtomic: cannot create temporary file");
  struct Cleanup {
    int& descriptor;
    const std::string& path;
    bool committed{false};
    ~Cleanup() { if (descriptor >= 0) close(descriptor); if (!committed) unlink(path.c_str()); }
  } cleanup{descriptor, pattern};
  if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) return fail(error, "Fs.WriteAtomic: cannot protect file descriptor");
  for (std::size_t offset = 0; offset < data.size();) {
    const auto written = write(descriptor, data.data() + offset, data.size() - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) return fail(error, "Fs.WriteAtomic: file write failed");
    offset += static_cast<std::size_t>(written);
  }
  if (fsync(descriptor) != 0) return fail(error, "Fs.WriteAtomic: file flush failed");
  const bool closed = close(descriptor) == 0;
  descriptor = -1;
  if (!closed) return fail(error, "Fs.WriteAtomic: file close failed");
  if (rename(pattern.c_str(), destination.c_str()) != 0)
    return fail(error, "Fs.WriteAtomic: destination replacement failed");
  cleanup.committed = true;
#endif
  return true;
}

bool write_atomic(const Value* args, const std::uint8_t arity, Value& out, std::string& error, const bool bytes) {
  try {
    std::filesystem::path path;
    if (arity != 2 || !path_argument(args[0], path, error)) return false;
    std::string data;
    if (bytes) {
      if (!args[1].is_struct() || args[1].field_count() > kByteLimit)
        return fail(error, "Fs.WriteBytesAtomic: expected a bounded byte array");
      data.reserve(args[1].field_count());
      for (std::size_t index = 0; index < args[1].field_count(); ++index) {
        const auto& byte = args[1].field(index);
        if (!byte.is_number() || !std::isfinite(byte.number) || byte.number < 0 || byte.number > 255 ||
            std::trunc(byte.number) != byte.number) return fail(error, "Fs.WriteBytesAtomic: expected integer bytes in 0..255");
        data.push_back(static_cast<char>(static_cast<unsigned char>(byte.number)));
      }
    } else {
      if (!args[1].is_string() || args[1].text.size() > kTextLimit)
        return fail(error, "Fs.WriteAtomic: expected text of at most 64 MiB");
      data = args[1].text;
    }
    if (!replace_file(path, data, error)) return false;
    out = Value::null_of();
    return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Fs.WriteAtomic: ") + failure.what()); }
}
bool write_text_atomic(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  return write_atomic(args, arity, out, error, false);
}
bool write_bytes_atomic(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  return write_atomic(args, arity, out, error, true);
}

bool stat_file(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  try {
    std::filesystem::path path;
    if (arity != 1 || !path_argument(args[0], path, error)) return false;
    std::error_code failure;
    const auto status = std::filesystem::status(path, failure);
    if (failure && failure != std::errc::no_such_file_or_directory) return fail(error, "Fs.Stat: cannot inspect path");
    const bool exists = std::filesystem::exists(status);
    const bool regular = std::filesystem::is_regular_file(status);
    std::int64_t size = 0;
    std::int64_t modified = 0;
    if (exists) {
      if (regular) {
        const auto bytes = std::filesystem::file_size(path);
        if (bytes > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()))
          return fail(error, "Fs.Stat: file size outside number range");
        size = static_cast<std::int64_t>(bytes);
      }
#ifdef _WIN32
      const HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      if (file == INVALID_HANDLE_VALUE) return fail(error, "Fs.Stat: cannot read file metadata");
      BY_HANDLE_FILE_INFORMATION information{};
      const bool inspected = GetFileInformationByHandle(file, &information) != 0;
      CloseHandle(file);
      if (!inspected) return fail(error, "Fs.Stat: cannot read file metadata");
      const auto ticks = (static_cast<std::uint64_t>(information.ftLastWriteTime.dwHighDateTime) << 32) |
                         information.ftLastWriteTime.dwLowDateTime;
      modified = static_cast<std::int64_t>(ticks / 10000) - 11644473600000LL;
#else
      const auto stamp = std::filesystem::last_write_time(path);
      using Milliseconds = std::chrono::duration<long double, std::milli>;
      const auto epoch = std::chrono::file_clock::to_sys(std::chrono::file_clock::time_point{});
      const long double milliseconds = Milliseconds(stamp.time_since_epoch()).count() + Milliseconds(epoch.time_since_epoch()).count();
      if (milliseconds < static_cast<long double>(std::numeric_limits<std::int64_t>::min()) ||
          milliseconds >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
        return fail(error, "Fs.Stat: timestamp outside number range");
      modified = static_cast<std::int64_t>(milliseconds);
#endif
    }
    out = Value::struct_of({boolean(exists), boolean(std::filesystem::is_directory(status)), boolean(regular),
                           Value::integer_of(size), Value::integer_of(modified)});
    return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Fs.Stat: ") + failure.what()); }
}
}  // namespace

void add_cli_files(std::vector<Entry>& table) {
  table.insert(table.end(), {{"fs::ReadText", 2, read_text, true}, {"fs::ReadBytes", 2, read_bytes, true},
    {"fs::WriteAtomic", 2, write_text_atomic, true}, {"fs::WriteBytesAtomic", 2, write_bytes_atomic, true},
    {"fs::Stat", 1, stat_file, true}});
}
}  // namespace clpp::stdlib::host
