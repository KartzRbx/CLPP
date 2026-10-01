// @clpp.io — the console: printing without a newline, reading lines, numbers and single keys,
// colors, cursor movement, clearing the screen.
//
// Colors and cursor control use ANSI escape sequences; on Windows the console's virtual-terminal
// mode is switched on the first time they are used. When the output is not a terminal (a pipe, a
// file, the test suite), color and cursor calls write nothing, so logs stay clean.

#include "stdlib/host.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace clpp::stdlib::host {

namespace {

bool g_end_of_input = false;

[[nodiscard]] std::string format_number(const double number) {
  if (std::isfinite(number) && number == std::trunc(number) && std::fabs(number) < 9007199254740992.0) {
    return std::to_string(static_cast<long long>(number));
  }
  char buffer[64];
  const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), number, std::chars_format::general);
  return result.ec == std::errc{} ? std::string(buffer, result.ptr) : std::string{};
}

// Same text post() prints.
[[nodiscard]] std::string format_value(const Value& value, const bool nested = false) {
  if (value.is_string()) {
    return nested ? "\"" + value.text + "\"" : value.text;
  }
  if (value.is_vector()) {
    std::string text = "(" + format_number(value.number) + ", " + format_number(value.y);
    if (value.dims >= 3) {
      text += ", " + format_number(value.z);
    }
    if (value.dims >= 4) {
      text += ", " + format_number(value.w);
    }
    return text + ")";
  }
  if (value.is_struct() && value.number == 65534 && value.fields.size() == 1) {
    return format_value(value.fields[0], nested);
  }
  if (value.is_struct()) {
    const bool dictionary = value.number == 65535;
    const bool list = value.number == 0;
    std::string text = dictionary ? "{" : list ? "[" : "(";
    for (std::size_t index = 0; index < value.fields.size(); index += dictionary ? 2 : 1) {
      if (index != 0) {
        text += ", ";
      }
      if (dictionary && index + 1 < value.fields.size()) {
        text += format_value(value.fields[index]) + ": " + format_value(value.fields[index + 1], true);
      } else {
        text += format_value(value.fields[index], true);
      }
    }
    return text + (dictionary ? "}" : list ? "]" : ")");
  }
  return format_number(value.number);
}

[[nodiscard]] bool stdout_is_terminal() {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

// ANSI sequences only reach a real terminal.
void ansi(const std::string_view sequence) {
  if (!stdout_is_terminal()) {
    return;
  }
#ifdef _WIN32
  static const bool enabled = [] {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0) {
      SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    return true;
  }();
  (void)enabled;
#endif
  std::cout << sequence;
  std::cout.flush();
}

[[nodiscard]] bool read_line(std::string& line) {
  std::cout.flush();
  if (!std::getline(std::cin, line)) {
    g_end_of_input = true;
    line.clear();
    return false;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return true;
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(io_print) {  // (value, 0 stdout | 1 stderr)
  (void)out;
  (void)error;
  (void)arity;
  if (num(args[1]) != 0) {
    std::cerr << format_value(args[0]);
    std::cerr.flush();
  } else {
    std::cout << format_value(args[0]);
    std::cout.flush();
  }
  return true;
}

CLPP_NATIVE(io_read_line) {  // (prompt)
  (void)arity;
  (void)error;
  if (args[0].is_string() && !args[0].text.empty()) {
    std::cout << args[0].text;
  }
  std::string line;
  (void)read_line(line);
  out = Value::string_of(std::move(line));
  return true;
}

CLPP_NATIVE(io_read_number) {  // (prompt): asks again until the answer is a number
  (void)arity;
  for (;;) {
    if (args[0].is_string() && !args[0].text.empty()) {
      std::cout << args[0].text;
    }
    std::string line;
    if (!read_line(line)) {
      return fail(error, "Io.ReadNumber: end of input");
    }
    const std::size_t begin = line.find_first_not_of(" \t");
    const std::size_t end = line.find_last_not_of(" \t");
    if (begin == std::string::npos) {
      continue;
    }
    const std::string trimmed = line.substr(begin, end - begin + 1);
    char* stop = nullptr;
    const double number = std::strtod(trimmed.c_str(), &stop);
    if (stop != nullptr && *stop == '\0') {
      out = Value::number_of(number);
      return true;
    }
  }
}

CLPP_NATIVE(io_read_all) {
  (void)args;
  (void)arity;
  (void)error;
  std::string text((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
  g_end_of_input = true;
  out = Value::string_of(std::move(text));
  return true;
}

CLPP_NATIVE(io_end_of_input) {
  (void)args;
  (void)arity;
  (void)error;
  out = boolean(g_end_of_input);
  return true;
}

CLPP_NATIVE(io_read_key) {  // one key, without Enter and without echo
  (void)args;
  (void)arity;
  (void)error;
  std::cout.flush();
  std::string key;
#ifdef _WIN32
  const int first = _getwch();
  if (first == 0 || first == 0xE0) {  // arrows and function keys come as two codes
    const int second = _getwch();
    switch (second) {
      case 72:
        key = "Up";
        break;
      case 80:
        key = "Down";
        break;
      case 75:
        key = "Left";
        break;
      case 77:
        key = "Right";
        break;
      case 71:
        key = "Home";
        break;
      case 79:
        key = "End";
        break;
      case 83:
        key = "Delete";
        break;
      default:
        key = second >= 59 && second <= 68 ? "F" + std::to_string(second - 58) : "Unknown";
        break;
    }
  } else if (first == '\r') {
    key = "Enter";
  } else if (first == 27) {
    key = "Escape";
  } else if (first == 8) {
    key = "Backspace";
  } else if (first == '\t') {
    key = "Tab";
  } else if (first == ' ') {
    key = "Space";
  } else {
    wchar_t unit = static_cast<wchar_t>(first);
    char buffer[8];
    const int length = WideCharToMultiByte(CP_UTF8, 0, &unit, 1, buffer, sizeof(buffer), nullptr, nullptr);
    key.assign(buffer, static_cast<std::size_t>(std::max(0, length)));
  }
#else
  termios saved{};
  const bool terminal = tcgetattr(STDIN_FILENO, &saved) == 0;
  if (terminal) {
    termios raw = saved;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }
  const int c = std::getchar();
  if (c == EOF) {
    g_end_of_input = true;
  } else if (c == 27) {
    const int next = std::getchar();
    if (next == '[') {
      const int code = std::getchar();
      key = code == 'A' ? "Up" : code == 'B' ? "Down" : code == 'C' ? "Right" : code == 'D' ? "Left" : "Unknown";
    } else {
      key = "Escape";
    }
  } else if (c == '\n' || c == '\r') {
    key = "Enter";
  } else if (c == 127 || c == 8) {
    key = "Backspace";
  } else if (c == ' ') {
    key = "Space";
  } else if (c == '\t') {
    key = "Tab";
  } else {
    key.push_back(static_cast<char>(c));
    const auto lead = static_cast<unsigned char>(c);
    const int extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
    for (int index = 0; index < extra; ++index) {
      key.push_back(static_cast<char>(std::getchar()));
    }
  }
  if (terminal) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
  }
#endif
  out = Value::string_of(std::move(key));
  return true;
}

CLPP_NATIVE(io_key_available) {
  (void)args;
  (void)arity;
  (void)error;
#ifdef _WIN32
  out = boolean(_kbhit() != 0);
#else
  int waiting = 0;
  out = boolean(ioctl(STDIN_FILENO, FIONREAD, &waiting) == 0 && waiting > 0);
#endif
  return true;
}

CLPP_NATIVE(io_color) {  // (name, foreground 0 | background 1)
  (void)out;
  if (arity != 2 || !args[0].is_string()) {
    return fail(error, "Io.Color: expected a color name such as \"red\"");
  }
  struct Named {
    std::string_view name;
    int code;
  };
  static constexpr Named kColors[] = {{"black", 30},       {"red", 31},          {"green", 32},        {"yellow", 33},
                                      {"blue", 34},        {"magenta", 35},      {"cyan", 36},         {"white", 37},
                                      {"gray", 90},        {"grey", 90},         {"brightred", 91},    {"brightgreen", 92},
                                      {"brightyellow", 93}, {"brightblue", 94},  {"brightmagenta", 95}, {"brightcyan", 96},
                                      {"brightwhite", 97}};
  std::string name;
  for (const char c : args[0].text) {
    if (c != ' ' && c != '_' && c != '-') {
      name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
  }
  if (name == "bold") {
    ansi("\x1b[1m");
    return true;
  }
  if (name == "underline") {
    ansi("\x1b[4m");
    return true;
  }
  for (const Named& color : kColors) {
    if (color.name == name) {
      ansi("\x1b[" + std::to_string(color.code + (num(args[1]) != 0 ? 10 : 0)) + "m");
      return true;
    }
  }
  return fail(error, "Io.Color: unknown color \"" + args[0].text + "\"");
}

CLPP_NATIVE(io_rgb) {  // (r, g, b, background)
  (void)out;
  if (!numbers(args, arity, error, "Io.ColorRgb")) {
    return false;
  }
  const auto channel = [&](const int index) { return std::to_string(static_cast<int>(std::clamp(args[index].number, 0.0, 255.0))); };
  ansi(std::string("\x1b[") + (args[3].number != 0 ? "48" : "38") + ";2;" + channel(0) + ";" + channel(1) + ";" +
       channel(2) + "m");
  return true;
}

CLPP_NATIVE(io_control) {  // (which, a, b): 0 reset, 1 clear, 2 move to (a, b), 3 hide cursor, 4 show cursor, 5 clear line
  (void)out;
  (void)arity;
  (void)error;
  switch (static_cast<int>(num(args[0]))) {
    case 0:
      ansi("\x1b[0m");
      break;
    case 1:
      ansi("\x1b[2J\x1b[H");
      break;
    case 2:
      ansi("\x1b[" + std::to_string(static_cast<int>(num(args[2])) + 1) + ";" +
           std::to_string(static_cast<int>(num(args[1])) + 1) + "H");
      break;
    case 3:
      ansi("\x1b[?25l");
      break;
    case 4:
      ansi("\x1b[?25h");
      break;
    case 5:
      ansi("\r\x1b[2K");
      break;
    default:
      break;
  }
  return true;
}

CLPP_NATIVE(io_title) {
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Io.Title: expected a string");
  }
#ifdef _WIN32
  const std::string& text = args[0].text;
  const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
  SetConsoleTitleW(wide.c_str());
#else
  ansi("\x1b]0;" + args[0].text + "\x07");
#endif
  return true;
}

CLPP_NATIVE(io_size) {  // (0 columns | 1 rows)
  (void)arity;
  (void)error;
  int columns = 80;
  int rows = 25;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) != 0) {
    columns = info.srWindow.Right - info.srWindow.Left + 1;
    rows = info.srWindow.Bottom - info.srWindow.Top + 1;
  }
#else
  winsize size{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
    columns = size.ws_col;
    rows = size.ws_row;
  }
#endif
  out = Value::number_of(num(args[0]) == 0 ? columns : rows);
  return true;
}

CLPP_NATIVE(io_is_terminal) {
  (void)args;
  (void)arity;
  (void)error;
  out = boolean(stdout_is_terminal());
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kIoSource = R"clp(<< @clpp.io: the console. Colors and cursor calls do nothing when the output is not a terminal.

func Print(value) { io::Print(value, 0); }
func PrintError(value) { io::Print(value, 1); }
func ReadLine() -> string { return io::ReadLine(""); }
func Prompt(string question) -> string { return io::ReadLine(question); }
func ReadNumber(string question) -> float { return io::ReadNumber(question); }
func ReadAll() -> string { return io::ReadAll(); }
func EndOfInput() -> bool { return io::EndOfInput(); }
func ReadKey() -> string { return io::ReadKey(); }
func KeyAvailable() -> bool { return io::KeyAvailable(); }
func Color(string name) { io::Color(name, 0); }
func Background(string name) { io::Color(name, 1); }
func ColorRgb(int r, int g, int b) { io::Rgb(r, g, b, 0); }
func BackgroundRgb(int r, int g, int b) { io::Rgb(r, g, b, 1); }
func Reset() { io::Control(0, 0, 0); }
func Clear() { io::Control(1, 0, 0); }
func MoveTo(int column, int row) { io::Control(2, column, row); }
func HideCursor() { io::Control(3, 0, 0); }
func ShowCursor() { io::Control(4, 0, 0); }
func ClearLine() { io::Control(5, 0, 0); }
func Title(string text) { io::Title(text); }
func Columns() -> int { return io::Size(0); }
func Rows() -> int { return io::Size(1); }
func IsTerminal() -> bool { return io::IsTerminal(); }
)clp";

}  // namespace

void add_io(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"io::Print", 2, io_print, false},
                                {"io::ReadLine", 1, io_read_line, true},
                                {"io::ReadNumber", 1, io_read_number, true},
                                {"io::ReadAll", 0, io_read_all, true},
                                {"io::EndOfInput", 0, io_end_of_input, false},
                                {"io::ReadKey", 0, io_read_key, true},
                                {"io::KeyAvailable", 0, io_key_available, true},
                                {"io::Color", 2, io_color, false},
                                {"io::Rgb", 4, io_rgb, false},
                                {"io::Control", 3, io_control, false},
                                {"io::Title", 1, io_title, false},
                                {"io::Size", 1, io_size, false},
                                {"io::IsTerminal", 0, io_is_terminal, false},
                            });
}

std::string_view io_source() { return kIoSource; }

}  // namespace clpp::stdlib::host
