#ifndef _WIN32
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#include "clpp/detail/utf8_path.hpp"
#include "stdlib/host.hpp"
#include "clpp/stdlib.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char** environ;
#endif

namespace clpp::stdlib {
namespace {
std::atomic<int> exit_code{0};
}
int program_exit_code() { return exit_code.load(); }

namespace host {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaxBytes = 64 * 1024 * 1024;

// Older supported libc++ SDKs do not ship std::jthread. No stop token is needed
// here: the owned process/job is terminated before exceptional pipe joins.
class JoiningThread {
 public:
  JoiningThread() = default;
  template <typename Function> explicit JoiningThread(Function&& function)
      : m_thread(std::forward<Function>(function)) {}
  JoiningThread(JoiningThread&&) noexcept = default;
  JoiningThread& operator=(JoiningThread&& other) {
    if (this != &other) {
      if (m_thread.joinable()) m_thread.join();
      m_thread = std::move(other.m_thread);
    }
    return *this;
  }
  ~JoiningThread() { if (m_thread.joinable()) m_thread.join(); }
  void join() { m_thread.join(); }
 private:
  std::thread m_thread;
};

std::string utf8_path(const std::filesystem::path& path) {
  const auto text = path.generic_u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}
bool no_nul(const std::string& text) { return text.find('\0') == std::string::npos; }
bool integral_range(const Value& value, const double low, const double high) {
  return value.is_number() && std::isfinite(value.number) && value.number >= low &&
         value.number <= high && std::trunc(value.number) == value.number;
}

struct Request {
  std::filesystem::path executable;
  std::filesystem::path directory;
  std::vector<std::string> arguments;
  std::vector<std::pair<std::string, std::string>> environment;
  bool inherit{true};
  std::string input;
  long long timeout{0};
  std::size_t limit{0};
};
struct Result {
  long long code{-1};
  std::string output;
  std::string error;
  bool timeout{false};
  std::atomic<bool> limited{false};
  std::atomic<bool> io_error{false};
};

bool decode(const Value* args, const std::uint8_t arity, Request& request, std::string& error) {
  if (arity != 3 || !args[0].is_string() || args[0].text.empty() || !no_nul(args[0].text) ||
      !args[1].is_struct() || !args[2].is_struct() || args[2].field_count() != 6) {
    return fail(error, "Process.Run: expected an executable path, argument array and Options");
  }
  const auto& options = args[2];
  if (!options.field(0).is_string() || !no_nul(options.field(0).text) || !options.field(1).is_string() ||
      options.field(1).text.size() > kMaxBytes || !integral_range(options.field(2), 1, 86400000) ||
      !integral_range(options.field(3), 1, kMaxBytes) || !options.field(4).is_struct() ||
      !options.field(5).is_boolean()) {
    return fail(error, "Process.Run: invalid Options (timeout 1..86400000 ms, output limit 1..67108864 bytes)");
  }
  request.directory = options.field(0).text.empty() ? std::filesystem::current_path()
      : std::filesystem::absolute(clpp::detail::utf8_path(options.field(0).text)).lexically_normal();
  request.executable = clpp::detail::utf8_path(args[0].text);
  if (!request.executable.is_absolute()) request.executable = request.directory / request.executable;
  request.executable = request.executable.lexically_normal();
  if (!std::filesystem::is_directory(request.directory)) return fail(error, "Process.Run: working directory does not exist");
  if (!std::filesystem::is_regular_file(request.executable)) return fail(error, "Process.Run: executable path does not exist");
  for (std::size_t index = 0; index < args[1].field_count(); ++index) {
    const auto& value = args[1].field(index);
    if (!value.is_string() || !no_nul(value.text)) return fail(error, "Process.Run: arguments must be strings without NUL bytes");
    request.arguments.push_back(value.text);
  }
  const auto& environment = options.field(4);
  for (std::size_t index = 0; index < environment.field_count(); ++index) {
    const auto& item = environment.field(index);
    if (!item.is_struct() || item.field_count() != 2 || !item.field(0).is_string() ||
        !item.field(1).is_string() || item.field(0).text.empty() || item.field(0).text.find('=') != std::string::npos ||
        !no_nul(item.field(0).text) || !no_nul(item.field(1).text)) {
      return fail(error, "Process.Run: environment entries require a nonempty name without '=' or NUL");
    }
    request.environment.emplace_back(item.field(0).text, item.field(1).text);
  }
  request.input = options.field(1).text;
  request.timeout = static_cast<long long>(options.field(2).number);
  request.limit = static_cast<std::size_t>(options.field(3).number);
  request.inherit = options.field(5).is_boolean() && options.field(5).number != 0;
  return true;
}

void capture(Result& result, std::string& destination, const char* bytes, const std::size_t count, const std::size_t limit) {
  const auto available = limit - destination.size();
  destination.append(bytes, std::min(count, available));
  if (count > available) result.limited.store(true);
}

#ifdef _WIN32
std::wstring wide(const std::string& text) {
  if (text.empty()) return {};
  if (text.size() > 2147483647) throw std::runtime_error("process text exceeds the Windows conversion range");
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (size == 0) throw std::runtime_error("invalid UTF-8 process argument or environment");
  std::wstring output(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), output.data(), size);
  return output;
}
std::wstring quoted(const std::wstring& text) {
  std::wstring output = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t c : text) {
    if (c == L'\\') { ++backslashes; continue; }
    output.append(c == L'\"' ? backslashes * 2 + 1 : backslashes, L'\\');
    output.push_back(c); backslashes = 0;
  }
  output.append(backslashes * 2, L'\\'); output.push_back(L'\"');
  return output;
}
struct Handle {
  HANDLE value{nullptr};
  Handle() = default;
  explicit Handle(HANDLE handle) : value(handle) {}
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  ~Handle() { close(); }
  void close() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = nullptr; }
};
struct Attributes {
  std::vector<unsigned char> bytes;
  LPPROC_THREAD_ATTRIBUTE_LIST list{nullptr};
  ~Attributes() { if (list) DeleteProcThreadAttributeList(list); }
};
struct EnvironmentLess {
  bool operator()(const std::wstring& a, const std::wstring& b) const {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
  }
};
std::vector<wchar_t> child_environment(const Request& request) {
  std::map<std::wstring, std::wstring, EnvironmentLess> values;
  if (request.inherit) {
    wchar_t* block = GetEnvironmentStringsW();
    if (!block) throw std::runtime_error("cannot read process environment");
    struct BlockGuard { wchar_t* value; ~BlockGuard() { FreeEnvironmentStringsW(value); } } guard{block};
    for (const wchar_t* entry = block; *entry; entry += std::wcslen(entry) + 1) {
      std::wstring item(entry);
      const auto equal = item.find(L'=', item.front() == L'=' ? 1 : 0);
      if (equal != std::wstring::npos) values[item.substr(0, equal)] = item.substr(equal + 1);
    }
  }
  for (const auto& [name, value] : request.environment) values[wide(name)] = wide(value);
  std::vector<wchar_t> block;
  for (const auto& [name, value] : values) {
    const auto item = name + L"=" + value;
    block.insert(block.end(), item.begin(), item.end()); block.push_back(L'\0');
  }
  block.push_back(L'\0'); if (values.empty()) block.push_back(L'\0');
  return block;
}
bool execute(const Request& request, Result& result, std::string& error) {
  const auto windows_error = [&](const char* what) { return fail(error, std::string("Process.Run: ") + what + " (Windows error " + std::to_string(GetLastError()) + ")"); };
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  Handle in_read, in_write, out_read, out_write, err_read, err_write;
  if (!CreatePipe(&in_read.value, &in_write.value, &security, 0) ||
      !CreatePipe(&out_read.value, &out_write.value, &security, 0) ||
      !CreatePipe(&err_read.value, &err_write.value, &security, 0) ||
      !SetHandleInformation(in_write.value, HANDLE_FLAG_INHERIT, 0) ||
      !SetHandleInformation(out_read.value, HANDLE_FLAG_INHERIT, 0) ||
      !SetHandleInformation(err_read.value, HANDLE_FLAG_INHERIT, 0)) return windows_error("cannot create pipes");
  Handle job(CreateJobObjectW(nullptr, nullptr));
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return windows_error("cannot create owned process job");
  Attributes attributes;
  SIZE_T size = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
  attributes.bytes.resize(size);
  auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.bytes.data());
  if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) return windows_error("cannot initialize process attributes");
  attributes.list = list;
  HANDLE inherited[] = {in_read.value, out_write.value, err_write.value};
  if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr)) return windows_error("cannot restrict inherited handles");
  auto command = quoted(request.executable.native());
  for (const auto& arg : request.arguments) { command.push_back(L' '); command += quoted(wide(arg)); }
  if (command.size() >= 32767) return fail(error, "Process.Run: Windows command line exceeds 32766 UTF-16 units");
  auto environment = child_environment(request);
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = in_read.value;
  startup.StartupInfo.hStdOutput = out_write.value;
  startup.StartupInfo.hStdError = err_write.value;
  startup.lpAttributeList = list;
  PROCESS_INFORMATION info{};
  if (!CreateProcessW(request.executable.c_str(), command.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
      environment.data(), request.directory.c_str(), &startup.StartupInfo, &info)) return windows_error("cannot start executable");
  Handle process(info.hProcess), thread(info.hThread);
  if (!AssignProcessToJobObject(job.value, process.value)) {
    const auto reason = GetLastError(); TerminateProcess(process.value, 1); WaitForSingleObject(process.value, INFINITE);
    SetLastError(reason); return windows_error("cannot own child process");
  }
  in_read.close(); out_write.close(); err_write.close();
  const auto read = [&](HANDLE handle, std::string& target) {
    char buffer[8192]; DWORD count = 0;
    while (ReadFile(handle, buffer, sizeof(buffer), &count, nullptr) && count != 0) {
      capture(result, target, buffer, count, request.limit);
      if (result.limited.load()) break;
    }
  };
  // Start all pipes concurrently: either output stream may fill before stdin is consumed.
  JoiningThread output, errors, input;
  try {
  output = JoiningThread([&] { try { read(out_read.value, result.output); } catch (...) { result.io_error.store(true); } });
  errors = JoiningThread([&] { try { read(err_read.value, result.error); } catch (...) { result.io_error.store(true); } });
  input = JoiningThread([&] {
    std::size_t offset = 0;
    while (offset < request.input.size()) {
      DWORD written = 0;
      const auto count = static_cast<DWORD>(std::min<std::size_t>(request.input.size() - offset, 8192));
      if (!WriteFile(in_write.value, request.input.data() + offset, count, &written, nullptr) || written == 0) break;
      offset += written;
    }
    in_write.close();
  });
  } catch (...) {
    TerminateJobObject(job.value, 1); WaitForSingleObject(process.value, INFINITE); job.close();
    throw;
  }
  const auto started = Clock::now();
  if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
    TerminateJobObject(job.value, 1); result.io_error.store(true);
  }
  while (WaitForSingleObject(process.value, 10) == WAIT_TIMEOUT) {
    if (result.io_error.load() || result.limited.load() || std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count() >= request.timeout) {
      result.timeout = !result.limited.load() && !result.io_error.load(); TerminateJobObject(job.value, 1); break;
    }
  }
  WaitForSingleObject(process.value, INFINITE);
  DWORD code = 0;
  if (!GetExitCodeProcess(process.value, &code)) result.io_error.store(true);
  result.code = code;
  // Descendants are owned too: they cannot keep the capture pipes open after the leader exits.
  TerminateJobObject(job.value, 1); job.close();
  input.join(); output.join(); errors.join();
  if (result.timeout || result.limited.load()) result.code = -1;
  if (result.io_error.load()) return windows_error("process I/O failed");
  return true;
}
#else
struct Fd {
  int value{-1};
  Fd() = default;
  explicit Fd(int fd) : value(fd) {}
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  ~Fd() { close(); }
  void close() { if (value >= 0) ::close(value); value = -1; }
};
bool pipe_pair(Fd& read, Fd& write) {
  int pair[2]; if (pipe(pair) != 0) return false;
  read.value = pair[0]; write.value = pair[1];
  return fcntl(pair[0], F_SETFD, FD_CLOEXEC) != -1 && fcntl(pair[1], F_SETFD, FD_CLOEXEC) != -1;
}
bool execute(const Request& request, Result& result, std::string& error) {
  Fd in_read, in_write, out_read, out_write, err_read, err_write;
  if (!pipe_pair(in_read, in_write) || !pipe_pair(out_read, out_write) || !pipe_pair(err_read, err_write)) return fail(error, "Process.Run: cannot create pipes");
#ifdef __APPLE__
  // Darwin supports disabling SIGPIPE on this descriptor without changing the
  // host application's process-wide signal disposition.
  if (fcntl(in_write.value, F_SETNOSIGPIPE, 1) == -1) return fail(error, "Process.Run: cannot protect the input pipe");
#endif
  std::vector<std::string> arguments{request.executable.native()};
  arguments.insert(arguments.end(), request.arguments.begin(), request.arguments.end());
  std::vector<char*> argv;
  for (auto& arg : arguments) argv.push_back(arg.data()); argv.push_back(nullptr);
  std::map<std::string, std::string> values;
  if (request.inherit) {
    for (char** entry = environ; *entry; ++entry) {
      const std::string item(*entry); const auto equal = item.find('=');
      if (equal != std::string::npos) values[item.substr(0, equal)] = item.substr(equal + 1);
    }
  }
  for (const auto& [name, value] : request.environment) values[name] = value;
  std::vector<std::string> environment;
  for (const auto& [name, value] : values) environment.push_back(name + "=" + value);
  std::vector<char*> envp;
  for (auto& entry : environment) envp.push_back(entry.data()); envp.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attributes;
  if (posix_spawn_file_actions_init(&actions) != 0) return fail(error, "Process.Run: cannot initialize spawn actions");
  struct ActionsGuard { posix_spawn_file_actions_t* value; ~ActionsGuard() { posix_spawn_file_actions_destroy(value); } } action_guard{&actions};
  if (posix_spawnattr_init(&attributes) != 0) return fail(error, "Process.Run: cannot initialize spawn attributes");
  struct AttrGuard { posix_spawnattr_t* value; ~AttrGuard() { posix_spawnattr_destroy(value); } } attribute_guard{&attributes};
  int failed = 0;
  failed |= posix_spawn_file_actions_adddup2(&actions, in_read.value, STDIN_FILENO);
  failed |= posix_spawn_file_actions_adddup2(&actions, out_write.value, STDOUT_FILENO);
  failed |= posix_spawn_file_actions_adddup2(&actions, err_write.value, STDERR_FILENO);
  failed |= posix_spawn_file_actions_addchdir_np(&actions, request.directory.c_str());
  failed |= posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  failed |= posix_spawnattr_setpgroup(&attributes, 0);
  if (failed) return fail(error, "Process.Run: cannot configure child process");
  pid_t child = -1;
  const int spawned = posix_spawn(&child, request.executable.c_str(), &actions, &attributes, argv.data(), envp.data());
  if (spawned) return fail(error, "Process.Run: cannot start executable (POSIX error " + std::to_string(spawned) + ")");
  struct ChildGuard { pid_t pid; ~ChildGuard() { if (pid > 0) { kill(-pid, SIGKILL); while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {} } } } child_guard{child};
  in_read.close(); out_write.close(); err_write.close();
  const auto read_pipe = [&](int fd, std::string& target) {
    char buffer[8192];
    while (true) {
      const auto count = read(fd, buffer, sizeof(buffer));
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) break;
      capture(result, target, buffer, static_cast<std::size_t>(count), request.limit);
      if (result.limited.load()) break;
    }
  };
  JoiningThread output, errors, input;
  try {
  output = JoiningThread([&] { try { read_pipe(out_read.value, result.output); } catch (...) { result.io_error.store(true); } });
  errors = JoiningThread([&] { try { read_pipe(err_read.value, result.error); } catch (...) { result.io_error.store(true); } });
  input = JoiningThread([&] {
    sigset_t mask; sigemptyset(&mask); sigaddset(&mask, SIGPIPE); pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    std::size_t offset = 0;
    while (offset < request.input.size()) {
      const auto count = write(in_write.value, request.input.data() + offset, request.input.size() - offset);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) break;
      offset += static_cast<std::size_t>(count);
    }
    in_write.close();
  });
  } catch (...) { kill(-child, SIGKILL); throw; }
  const auto started = Clock::now();
  while (true) {
    siginfo_t state{};
    if (waitid(P_PID, static_cast<id_t>(child), &state, WEXITED | WNOHANG | WNOWAIT) == 0 && state.si_pid == child) break;
    if (result.io_error.load() || result.limited.load() || std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count() >= request.timeout) {
      result.timeout = !result.limited.load() && !result.io_error.load(); break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // Keep the leader unreaped until the owned group is killed, preventing PID reuse.
  kill(-child, SIGKILL);
  int status = 0; while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
  child_guard.pid = -1;
  input.join(); output.join(); errors.join();
  result.code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  if (result.timeout || result.limited.load()) result.code = -1;
  if (result.io_error.load()) return fail(error, "Process.Run: process I/O failed");
  return true;
}
#endif

bool process_run(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  try {
    Request request; if (!decode(args, arity, request, error)) return false;
    Result result; if (!execute(request, result, error)) return false;
    out = Value::struct_of({Value::integer_of(result.code), Value::string_of(std::move(result.output)),
      Value::string_of(std::move(result.error)), boolean(result.timeout), boolean(result.limited.load())});
    return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Process.Run: ") + failure.what()); }
}
bool set_exit(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  if (arity != 1 || !integral_range(args[0], 0, 255)) return fail(error, "Process.SetExitCode: expected an integer in 0..255");
  exit_code.store(static_cast<int>(args[0].number)); out = Value::null_of(); return true;
}
bool write_error(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  if (arity != 1 || !args[0].is_string()) return fail(error, "Io.Error: expected a string");
  std::cerr.write(args[0].text.data(), static_cast<std::streamsize>(args[0].text.size())); std::cerr.flush();
  if (!std::cerr) return fail(error, "Io.Error: failed to write stderr");
  out = Value::null_of(); return true;
}
bool executable_path(const Value*, const std::uint8_t, Value& out, std::string& error) {
  try {
    std::filesystem::path path;
#ifdef _WIN32
    std::vector<wchar_t> buffer(512);
    while (true) {
      const auto count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
      if (count == 0) return fail(error, "Process.ExecutablePath: cannot locate executable");
      if (count < buffer.size()) { path = std::wstring(buffer.data(), count); break; }
      if (buffer.size() >= 32768) return fail(error, "Process.ExecutablePath: executable path too long");
      buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t length = 0;
    _NSGetExecutablePath(nullptr, &length);
    std::vector<char> buffer(length);
    if (_NSGetExecutablePath(buffer.data(), &length) != 0) return fail(error, "Process.ExecutablePath: cannot locate executable");
    path = std::filesystem::canonical(buffer.data());
#elif defined(__linux__)
    path = std::filesystem::read_symlink("/proc/self/exe");
#else
    return fail(error, "Process.ExecutablePath: unsupported platform");
#endif
    out = Value::string_of(utf8_path(path)); return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Process.ExecutablePath: ") + failure.what()); }
}
bool equivalent_paths(const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  try {
    if (arity != 2 || !args[0].is_string() || !args[1].is_string() || !no_nul(args[0].text) || !no_nul(args[1].text))
      return fail(error, "Path.Equivalent: expected paths without NUL bytes");
    std::error_code failure;
    const auto first = clpp::detail::utf8_path(args[0].text);
    const auto second = clpp::detail::utf8_path(args[1].text);
    if (!std::filesystem::exists(first) || !std::filesystem::exists(second))
      return fail(error, "Path.Equivalent: both paths must exist");
    const bool equivalent = std::filesystem::equivalent(first, second, failure);
    if (failure) return fail(error, "Path.Equivalent: cannot compare paths; both must exist");
    out = boolean(equivalent); return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Path.Equivalent: ") + failure.what()); }
}
#ifdef _WIN32
// Resolve the existing prefix using real Windows handles, including directories
// and reparse points, then normalize any nonexistent suffix lexically.
std::filesystem::path canonical_windows(const std::filesystem::path& original) {
  auto prefix = std::filesystem::absolute(original);
  std::filesystem::path tail;
  while (true) {
    Handle file(CreateFileW(prefix.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (file.value != INVALID_HANDLE_VALUE) {
      std::vector<wchar_t> buffer(512);
      while (true) {
        const DWORD count = GetFinalPathNameByHandleW(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
        if (count == 0) throw std::runtime_error("cannot resolve existing path");
        if (count < buffer.size()) {
          std::wstring text(buffer.data(), count);
          if (text.starts_with(L"\\\\?\\UNC\\")) text = L"\\\\" + text.substr(8);
          else if (text.starts_with(L"\\\\?\\")) text.erase(0, 4);
          return (tail.empty() ? std::filesystem::path(text) : std::filesystem::path(text) / tail).lexically_normal();
        }
        if (count >= 32768) throw std::runtime_error("canonical path too long");
        buffer.resize(static_cast<std::size_t>(count) + 1);
      }
    }
    const DWORD failure = GetLastError();
    if (failure != ERROR_FILE_NOT_FOUND && failure != ERROR_PATH_NOT_FOUND)
      throw std::runtime_error("cannot open existing path for resolution");
    const auto parent = prefix.parent_path();
    if (parent.empty() || parent == prefix) throw std::runtime_error("cannot resolve path root");
    tail = prefix.filename() / tail;
    prefix = parent;
  }
}
#endif
bool path_operation(const Value* args, const std::uint8_t arity, Value& out, std::string& error, const int operation) {
  try {
    for (std::uint8_t index = 0; index < arity; ++index) if (!args[index].is_string() || !no_nul(args[index].text)) return fail(error, "Path: expected strings without NUL bytes");
    std::filesystem::path path;
    if (operation == 0) path = std::filesystem::current_path();
    else if (operation == 1) path = std::filesystem::temp_directory_path();
    else {
      path = clpp::detail::utf8_path(args[0].text);
      if (operation == 3) path /= clpp::detail::utf8_path(args[1].text);
      if (operation == 4 && !path.is_absolute()) {
        auto base = args[1].text.empty() ? std::filesystem::current_path() : std::filesystem::absolute(clpp::detail::utf8_path(args[1].text));
        path = base / path;
      }
      if (operation == 5) path = path.parent_path();
      if (operation == 6) path = path.filename();
      if (operation == 7) { out = boolean(path.is_absolute()); return true; }
      if (operation == 8) {
#ifdef _WIN32
        path = canonical_windows(path);
#else
        path = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
#endif
      }
    }
    out = Value::string_of(utf8_path(path.lexically_normal())); return true;
  } catch (const std::exception& failure) { return fail(error, std::string("Path: ") + failure.what()); }
}
#define PATH_NATIVE(name, operation) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error) { return path_operation(args, arity, out, error, operation); }
PATH_NATIVE(path_cwd, 0)
PATH_NATIVE(path_temp, 1)
PATH_NATIVE(path_normalize, 2)
PATH_NATIVE(path_join, 3)
PATH_NATIVE(path_absolute, 4)
PATH_NATIVE(path_parent, 5)
PATH_NATIVE(path_filename, 6)
PATH_NATIVE(path_is_absolute, 7)
PATH_NATIVE(path_canonical, 8)
#undef PATH_NATIVE

constexpr std::string_view kProcessSource = R"clp(<< Processes use explicit executable paths, never an implicit shell or PATH search.
export def EnvironmentEntry { string name; string value; }
export def Options {
  string workingDirectory = "";
  string input = "";
  number timeoutMs = 60000;
  number maxOutputBytes = 16777216;
  array<EnvironmentEntry> environment = array<EnvironmentEntry>();
  bool inheritEnvironment = true;
}
export def Result { number exitCode; string stdout; string stderr; bool timedOut; bool outputLimited; }
export Result Run(string executable, array<string> arguments, Options options) { return process::Run(executable, arguments, options); }
export void SetExitCode(number code) { process::SetExitCode(code); }
export string ExecutablePath() { return process::ExecutablePath(); }
)clp";
constexpr std::string_view kPathSource = R"clp(<< Lexical path operations do not change the process working directory.
export string CurrentDirectory() { return path::CurrentDirectory(); }
export string TempDirectory() { return path::TempDirectory(); }
export string Normalize(string value) { return path::Normalize(value); }
export string Join(string base, string value) { return path::Join(base, value); }
export string Absolute(string value, string base) { return path::Absolute(value, base); }
export string Parent(string value) { return path::Parent(value); }
export string FileName(string value) { return path::FileName(value); }
export bool IsAbsolute(string value) { return path::IsAbsolute(value); }
export string Canonical(string value) { return path::Canonical(value); }
export bool Equivalent(string first, string second) { return path::Equivalent(first, second); }
)clp";
}  // namespace

void reset_process_status() { exit_code.store(0); }
void add_tooling(std::vector<Entry>& table) {
  table.insert(table.end(), {
    {"process::Run", 3, process_run, true}, {"process::SetExitCode", 1, set_exit, true},
    {"path::CurrentDirectory", 0, path_cwd, true}, {"path::TempDirectory", 0, path_temp, true},
    {"path::Normalize", 1, path_normalize, false}, {"path::Join", 2, path_join, false},
    {"path::Absolute", 2, path_absolute, true}, {"path::Parent", 1, path_parent, false},
    {"path::FileName", 1, path_filename, false}, {"path::IsAbsolute", 1, path_is_absolute, false},
    {"io::Error", 1, write_error, true},
    {"process::ExecutablePath", 0, executable_path, true},
    {"path::Canonical", 1, path_canonical, true}, {"path::Equivalent", 2, equivalent_paths, true},
  });
}
std::string_view process_source() { return kProcessSource; }
std::string_view path_source() { return kPathSource; }
}  // namespace host
}  // namespace clpp::stdlib
