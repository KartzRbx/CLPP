// @clpp.input — desktop automation for macros and UI testing, in the spirit of AutoHotkey and
// Python's pyautogui.
//
// It drives the real mouse and keyboard (move, click, drag, scroll, press keys, type text, send
// hotkeys) and reads the screen (mouse position, pixel color, a captured region as a @clpp.gfx
// image), so a CL++ program can record and replay macros, automate repetitive UI steps, write
// end-to-end tests of other apps, or build a clicker.
//
// Everything here acts visibly on the user's own session: the cursor moves where you can see it,
// keystrokes go to the focused window. Nothing is hidden, and every native is refused inside
// actor(...). Windows is the full implementation (Win32 SendInput / GetCursorPos / GetPixel); on
// other systems these report that they are not supported yet.

#include "stdlib/host.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace clpp::stdlib::host {

namespace {

#ifndef _WIN32
[[nodiscard]] bool unsupported(std::string& error) {
  error = "@clpp.input has no backend on this system yet";
  return false;
}
#endif

#ifdef _WIN32

// Virtual-key code for a key name ("a", "enter", "f5", "left", "ctrl", ...). 0 = unknown.
[[nodiscard]] WORD vk_for(std::string_view name) {
  std::string key;
  for (const char c : name) {
    if (c != ' ' && c != '_' && c != '-') {
      key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
  }
  if (key.size() == 1) {
    const char c = key[0];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      return static_cast<WORD>(std::toupper(static_cast<unsigned char>(c)));
    }
  }
  if (key.size() >= 2 && key[0] == 'f' && std::isdigit(static_cast<unsigned char>(key[1])) != 0) {
    const int number = std::atoi(key.c_str() + 1);
    if (number >= 1 && number <= 24) {
      return static_cast<WORD>(VK_F1 + number - 1);
    }
  }
  static const std::unordered_map<std::string, WORD> kNamed = {
      {"enter", VK_RETURN},   {"return", VK_RETURN},  {"escape", VK_ESCAPE},  {"esc", VK_ESCAPE},
      {"space", VK_SPACE},    {"tab", VK_TAB},        {"backspace", VK_BACK}, {"delete", VK_DELETE},
      {"del", VK_DELETE},     {"insert", VK_INSERT},  {"home", VK_HOME},      {"end", VK_END},
      {"pageup", VK_PRIOR},   {"pagedown", VK_NEXT},  {"left", VK_LEFT},      {"right", VK_RIGHT},
      {"up", VK_UP},          {"down", VK_DOWN},      {"ctrl", VK_CONTROL},   {"control", VK_CONTROL},
      {"shift", VK_SHIFT},    {"alt", VK_MENU},       {"win", VK_LWIN},       {"windows", VK_LWIN},
      {"cmd", VK_LWIN},       {"capslock", VK_CAPITAL}, {"printscreen", VK_SNAPSHOT},
      {"plus", VK_OEM_PLUS},  {"minus", VK_OEM_MINUS}, {"comma", VK_OEM_COMMA}, {"period", VK_OEM_PERIOD},
  };
  const auto found = kNamed.find(key);
  return found == kNamed.end() ? 0 : found->second;
}

void key_event(const WORD vk, const bool down) {
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  const WORD extended[] = {VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_INSERT, VK_DELETE};
  if (std::find(std::begin(extended), std::end(extended), vk) != std::end(extended)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  SendInput(1, &input, sizeof(INPUT));
}

// One Unicode code point as a key press (for Input.Type: any character, not just the ones on a key).
void unicode_event(const wchar_t unit, const bool down) {
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wScan = unit;
  input.ki.dwFlags = KEYEVENTF_UNICODE | (down ? 0 : KEYEVENTF_KEYUP);
  SendInput(1, &input, sizeof(INPUT));
}

void type_text(const std::string& text) {
  const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(std::max(0, count)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
  for (const wchar_t unit : wide) {
    if (unit == L'\n') {
      key_event(VK_RETURN, true);
      key_event(VK_RETURN, false);
      continue;
    }
    if (unit == L'\t') {
      key_event(VK_TAB, true);
      key_event(VK_TAB, false);
      continue;
    }
    unicode_event(unit, true);
    unicode_event(unit, false);
  }
}

void mouse_button(const int button, const bool down) {
  INPUT input{};
  input.type = INPUT_MOUSE;
  if (button == 1) {
    input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
  } else if (button == 2) {
    input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
  } else {
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
  }
  SendInput(1, &input, sizeof(INPUT));
}

void move_to(const int x, const int y) { SetCursorPos(x, y); }

// Split "ctrl+shift+s" / "alt+f4" into virtual keys.
[[nodiscard]] std::vector<WORD> hotkey_keys(const std::string& combo) {
  std::vector<WORD> keys;
  std::size_t start = 0;
  while (start <= combo.size()) {
    std::size_t plus = combo.find('+', start);
    if (plus == start) {  // a literal '+' key
      plus = combo.find('+', start + 1);
    }
    const std::string piece = combo.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
    if (!piece.empty()) {
      if (const WORD vk = vk_for(piece)) {
        keys.push_back(vk);
      }
    }
    if (plus == std::string::npos) {
      break;
    }
    start = plus + 1;
  }
  return keys;
}

#endif  // _WIN32

#define CLPP_NATIVE(name) bool name([[maybe_unused]] const Value* args, [[maybe_unused]] const std::uint8_t arity, \
                                    [[maybe_unused]] Value& out, [[maybe_unused]] std::string& error)

CLPP_NATIVE(input_move) {  // (x, y)
  (void)out;
  if (!numbers(args, arity, error, "Input.Move")) {
    return false;
  }
#ifdef _WIN32
  move_to(static_cast<int>(args[0].number), static_cast<int>(args[1].number));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_move_by) {  // (dx, dy)
  (void)out;
  if (!numbers(args, arity, error, "Input.MoveBy")) {
    return false;
  }
#ifdef _WIN32
  POINT point{};
  GetCursorPos(&point);
  move_to(point.x + static_cast<int>(args[0].number), point.y + static_cast<int>(args[1].number));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_mouse) {  // (button, 0 click | 1 down | 2 up | 3 double)
  (void)out;
  if (!numbers(args, arity, error, "Input mouse")) {
    return false;
  }
#ifdef _WIN32
  const int button = std::clamp(static_cast<int>(args[0].number), 0, 2);
  switch (static_cast<int>(args[1].number)) {
    case 1:
      mouse_button(button, true);
      break;
    case 2:
      mouse_button(button, false);
      break;
    case 3:
      for (int press = 0; press < 2; ++press) {
        mouse_button(button, true);
        mouse_button(button, false);
      }
      break;
    default:
      mouse_button(button, true);
      mouse_button(button, false);
      break;
  }
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_scroll) {  // (amount): wheel notches, positive = up
  (void)out;
  if (!numbers(args, arity, error, "Input.Scroll")) {
    return false;
  }
#ifdef _WIN32
  INPUT input{};
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_WHEEL;
  input.mi.mouseData = static_cast<DWORD>(static_cast<int>(args[0].number) * WHEEL_DELTA);
  SendInput(1, &input, sizeof(INPUT));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_mouse_pos) {  // (0 x | 1 y)
#ifdef _WIN32
  POINT point{};
  GetCursorPos(&point);
  out = Value::number_of(num(args[0]) == 0 ? point.x : point.y);
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_key) {  // (name, 0 tap | 1 down | 2 up)
  (void)out;
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Input key: expected a key name such as \"enter\" or \"a\"");
  }
#ifdef _WIN32
  const WORD vk = vk_for(args[0].text);
  if (vk == 0) {
    return fail(error, "Input key: unknown key \"" + args[0].text + "\"");
  }
  switch (static_cast<int>(args[1].number)) {
    case 1:
      key_event(vk, true);
      break;
    case 2:
      key_event(vk, false);
      break;
    default:
      key_event(vk, true);
      key_event(vk, false);
      break;
  }
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_hotkey) {  // (combo like "ctrl+shift+s")
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Input.Hotkey: expected a combo such as \"ctrl+s\"");
  }
#ifdef _WIN32
  const std::vector<WORD> keys = hotkey_keys(args[0].text);
  if (keys.empty()) {
    return fail(error, "Input.Hotkey: no known keys in \"" + args[0].text + "\"");
  }
  for (const WORD vk : keys) {
    key_event(vk, true);
  }
  for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
    key_event(*it, false);
  }
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_type) {  // (text)
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Input.Type: expected a string");
  }
#ifdef _WIN32
  type_text(args[0].text);
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_pixel) {  // (x, y) -> 0xRRGGBB of the screen
  if (!numbers(args, arity, error, "Input.Pixel")) {
    return false;
  }
#ifdef _WIN32
  HDC screen = GetDC(nullptr);
  const COLORREF color = GetPixel(screen, static_cast<int>(args[0].number), static_cast<int>(args[1].number));
  ReleaseDC(nullptr, screen);
  if (color == CLR_INVALID) {
    out = Value::number_of(0);
    return true;
  }
  out = Value::number_of((static_cast<std::uint32_t>(GetRValue(color)) << 16U) |
                         (static_cast<std::uint32_t>(GetGValue(color)) << 8U) | GetBValue(color));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_screen) {  // (0 width | 1 height)
#ifdef _WIN32
  out = Value::number_of(GetSystemMetrics(num(args[0]) == 0 ? SM_CXSCREEN : SM_CYSCREEN));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_capture) {  // (x, y, w, h) -> a @clpp.gfx image handle
  if (!numbers(args, arity, error, "Input.Capture")) {
    return false;
  }
#ifdef _WIN32
  const int x = static_cast<int>(args[0].number);
  const int y = static_cast<int>(args[1].number);
  const int w = static_cast<int>(args[2].number);
  const int h = static_cast<int>(args[3].number);
  if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
    return fail(error, "Input.Capture: invalid size");
  }
  HDC screen = GetDC(nullptr);
  HDC memory = CreateCompatibleDC(screen);
  HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
  HGDIOBJ previous = SelectObject(memory, bitmap);
  BitBlt(memory, 0, 0, w, h, screen, x, y, SRCCOPY);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = w;
  info.bmiHeader.biHeight = -h;  // top-down
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
  GetDIBits(memory, bitmap, 0, static_cast<UINT>(h), pixels.data(), &info, DIB_RGB_COLORS);
  for (std::uint32_t& pixel : pixels) {
    pixel = 0xFF000000U | (pixel & 0x00FFFFFFU);  // BGRA from GDI is already 0xAARRGGBB order here
  }
  SelectObject(memory, previous);
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(nullptr, screen);
  out = Value::number_of(add_image(w, h, std::move(pixels)));
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_down) {  // (name) -> is this key currently held
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Input.IsDown: expected a key name");
  }
#ifdef _WIN32
  const WORD vk = vk_for(args[0].text);
  if (vk == 0) {
    return fail(error, "Input.IsDown: unknown key \"" + args[0].text + "\"");
  }
  out = boolean((GetAsyncKeyState(vk) & 0x8000) != 0);
  return true;
#else
  return unsupported(error);
#endif
}

CLPP_NATIVE(input_sleep) {  // (milliseconds): pause between macro steps
  (void)out;
  if (!numbers(args, arity, error, "Input.Wait")) {
    return false;
  }
  if (args[0].number > 0) {
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(args[0].number));
  }
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kInputSource = R"clp(<< @clpp.input: desktop automation for macros and UI testing (like AutoHotkey / pyautogui).
<< Moves the real mouse and keyboard and reads the screen, visibly, in the user's own session.
<< Every function is refused inside actor(...). Buttons: 0 left, 1 right, 2 middle.

func Move(float x, float y) { input::Move(x, y); }
func MoveBy(float dx, float dy) { input::MoveBy(dx, dy); }
func MouseX() -> int { return input::MousePos(0); }
func MouseY() -> int { return input::MousePos(1); }
func Click(int button = 0) { input::Mouse(button, 0); }
func DoubleClick(int button = 0) { input::Mouse(button, 3); }
func MouseDown(int button = 0) { input::Mouse(button, 1); }
func MouseUp(int button = 0) { input::Mouse(button, 2); }
func ClickAt(float x, float y, int button = 0) { input::Move(x, y); input::Mouse(button, 0); }
func Scroll(int amount) { input::Scroll(amount); }
func Press(string key) { input::Key(key, 0); }
func KeyDown(string key) { input::Key(key, 1); }
func KeyUp(string key) { input::Key(key, 2); }
func Hotkey(string combo) { input::Hotkey(combo); }
func Type(string text) { input::Type(text); }
func IsDown(string key) -> bool { return input::IsDown(key); }
func Pixel(float x, float y) -> int { return input::Pixel(x, y); }
func ScreenWidth() -> int { return input::Screen(0); }
func ScreenHeight() -> int { return input::Screen(1); }
func Capture(float x, float y, float width, float height) -> int { return input::Capture(x, y, width, height); }
func Wait(float milliseconds) { input::Wait(milliseconds); }
)clp";

}  // namespace

void add_input(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"input::Move", 2, input_move, true},
                                {"input::MoveBy", 2, input_move_by, true},
                                {"input::MousePos", 1, input_mouse_pos, true},
                                {"input::Mouse", 2, input_mouse, true},
                                {"input::Scroll", 1, input_scroll, true},
                                {"input::Key", 2, input_key, true},
                                {"input::Hotkey", 1, input_hotkey, true},
                                {"input::Type", 1, input_type, true},
                                {"input::IsDown", 1, input_down, true},
                                {"input::Pixel", 2, input_pixel, true},
                                {"input::Screen", 1, input_screen, true},
                                {"input::Capture", 4, input_capture, true},
                                {"input::Wait", 1, input_sleep, true},
                            });
}

std::string_view input_source() { return kInputSource; }

}  // namespace clpp::stdlib::host
