// @clpp.time — wall clock, monotonic clock, sleeping and dates.
// @clpp.fs (writing half) — write, append, exists, remove, makeDir, isDir.

#include "stdlib/host.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace clpp::stdlib::host {

namespace {

const auto g_program_start = std::chrono::steady_clock::now();

[[nodiscard]] std::tm local_now() {
  const std::time_t now = std::time(nullptr);
  std::tm parts{};
#ifdef _WIN32
  localtime_s(&parts, &now);
#else
  localtime_r(&now, &parts);
#endif
  return parts;
}

[[nodiscard]] std::string format_time(const char* pattern) {
  const std::tm parts = local_now();
  char buffer[256];
  const std::size_t written = std::strftime(buffer, sizeof(buffer), pattern, &parts);
  return std::string(buffer, written);
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(time_now) {
  (void)args;
  (void)arity;
  (void)error;
  const auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
  out = Value::number_of(std::chrono::duration<double>(since_epoch).count());
  return true;
}

CLPP_NATIVE(time_clock) {
  (void)args;
  (void)arity;
  (void)error;
  out = Value::number_of(std::chrono::duration<double>(std::chrono::steady_clock::now() - g_program_start).count());
  return true;
}

CLPP_NATIVE(time_sleep) {
  (void)out;
  if (!numbers(args, arity, error, "Time.Sleep")) {
    return false;
  }
  const double milliseconds = args[0].number;
  if (milliseconds > 0) {
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(milliseconds));
  }
  return true;
}

CLPP_NATIVE(time_format) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Time.Format: expected a pattern such as \"%Y-%m-%d %H:%M\"");
  }
  if (args[0].text.size() > 128) {
    return fail(error, "Time.Format: pattern too long");
  }
  out = Value::string_of(format_time(args[0].text.c_str()));
  return true;
}

CLPP_NATIVE(time_part) {  // 0 year, 1 month, 2 day, 3 hour, 4 minute, 5 second, 6 weekday (0 = Sunday), 7 day of year
  (void)arity;
  (void)error;
  const std::tm parts = local_now();
  const int values[8] = {parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour,
                         parts.tm_min,         parts.tm_sec,     parts.tm_wday, parts.tm_yday + 1};
  const int which = static_cast<int>(num(args[0]));
  out = Value::number_of(which >= 0 && which < 8 ? values[which] : 0);
  return true;
}

// --- files --------------------------------------------------------------------------------------

[[nodiscard]] bool check_path(const Value* args, const std::uint8_t arity, const std::uint8_t strings,
                              std::string& error, const std::string_view what) {
  for (std::uint8_t index = 0; index < strings; ++index) {
    if (index >= arity || !args[index].is_string()) {
      return fail(error, std::string(what) + ": expected a string");
    }
  }
  if (args[0].text.find("..") != std::string::npos) {
    return fail(error, std::string(what) + ": paths with '..' are refused");
  }
  return true;
}

CLPP_NATIVE(fs_write) {  // (path, text, append)
  if (!check_path(args, arity, 2, error, "Fs.write")) {
    return false;
  }
  std::ofstream file(args[0].text, std::ios::binary | (num(args[2]) != 0 ? std::ios::app : std::ios::trunc));
  file.write(args[1].text.data(), static_cast<std::streamsize>(args[1].text.size()));
  out = boolean(static_cast<bool>(file));
  return true;
}

CLPP_NATIVE(fs_exists) {  // (path, 0 exists | 1 is directory)
  if (!check_path(args, arity, 1, error, "Fs.exists")) {
    return false;
  }
  std::error_code failure;
  const bool result = num(args[1]) != 0 ? std::filesystem::is_directory(args[0].text, failure)
                                        : std::filesystem::exists(args[0].text, failure);
  out = boolean(result && !failure);
  return true;
}

CLPP_NATIVE(fs_remove) {
  if (!check_path(args, arity, 1, error, "Fs.remove")) {
    return false;
  }
  std::error_code failure;
  out = boolean(std::filesystem::remove(args[0].text, failure) && !failure);  // a file or an empty folder
  return true;
}

CLPP_NATIVE(fs_make_dir) {
  if (!check_path(args, arity, 1, error, "Fs.makeDir")) {
    return false;
  }
  std::error_code failure;
  std::filesystem::create_directories(args[0].text, failure);
  out = boolean(!failure && std::filesystem::is_directory(args[0].text, failure));
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kTimeSource = R"clp(<< @clpp.time: clocks, sleeping and the local date.

func Now() -> float { return time::Now(); }
func Clock() -> float { return time::Clock(); }
func Sleep(float milliseconds) { time::Sleep(milliseconds); }
func Format(string pattern) -> string { return time::Format(pattern); }
func Date() -> string { return time::Format("%Y-%m-%d"); }
func TimeOfDay() -> string { return time::Format("%H:%M:%S"); }
func Year() -> int { return time::Part(0); }
func Month() -> int { return time::Part(1); }
func Day() -> int { return time::Part(2); }
func Hour() -> int { return time::Part(3); }
func Minute() -> int { return time::Part(4); }
func Second() -> int { return time::Part(5); }
func Weekday() -> int { return time::Part(6); }
func DayOfYear() -> int { return time::Part(7); }
)clp";

constexpr std::string_view kFsSource = R"clp(<< @clpp.fs: files. Paths containing ".." are refused, and code inside actor(...) cannot use files.

func read(string path) -> string { return fs::read(path); }
func size(string path) -> int { return fs::size(path); }
func list(string path) { return fs::list(path); }
func write(string path, string text) -> bool { return fs::Write(path, text, 0); }
func append(string path, string text) -> bool { return fs::Write(path, text, 1); }
func exists(string path) -> bool { return fs::Exists(path, 0); }
func isDir(string path) -> bool { return fs::Exists(path, 1); }
func remove(string path) -> bool { return fs::Remove(path); }
func makeDir(string path) -> bool { return fs::MakeDir(path); }
)clp";

}  // namespace

void add_time(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"time::Now", 0, time_now, false},
                                {"time::Clock", 0, time_clock, false},
                                {"time::Sleep", 1, time_sleep, false},
                                {"time::Format", 1, time_format, false},
                                {"time::Part", 1, time_part, false},
                            });
}

void add_files(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"fs::Write", 3, fs_write, true},
                                {"fs::Exists", 2, fs_exists, true},
                                {"fs::Remove", 1, fs_remove, true},
                                {"fs::MakeDir", 1, fs_make_dir, true},
                            });
}

std::string_view time_source() { return kTimeSource; }
std::string_view fs_source() { return kFsSource; }

}  // namespace clpp::stdlib::host
