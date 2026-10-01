// @clpp.window — a native window that shows the @clpp.gfx canvas, plus keyboard, mouse and timing.
//
// The program owns the loop:
//
//   Window.Open("Game", 800, 600);
//   while (Window.Frame()) {     // after your draw from the previous iteration: pace, present, input
//     Gfx.Clear(Gfx.DARK);
//     ...
//   }
//
// Windows uses Win32 directly (user32/gdi32, no extra DLLs). Other systems, and any system with
// the CLPP_HEADLESS environment variable set, run "headless": no window appears, everything else
// works (canvas, input simulation, timing) and Frame() returns false after a fixed number of
// frames (CLPP_HEADLESS=<n>, default 60), so tests, CI and tools never hang. Simulate* functions
// inject input; they work in both modes, which also makes UI automation possible.

#include "stdlib/host.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
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
#include <mmsystem.h>
#include <windowsx.h>
#endif

namespace clpp::stdlib::host {

namespace {

using Clock = std::chrono::steady_clock;

struct WindowState {
  bool open{false};
  bool closed{false};  // the user closed it (or Close was called)
  bool headless{false};
  long long frame_limit{60};
  std::string title;
  int fps{60};
  Clock::time_point started{};
  Clock::time_point last_frame{};
  double time{0};
  double delta{0};
  double measured_fps{0};
  Input input;
  // keys: canonical name -> down
  std::unordered_map<std::string, bool> keys;
  std::unordered_map<std::string, bool> keys_before;
  // input arriving between frames (OS messages and Simulate*), applied by Frame()
  double pending_x{0};
  double pending_y{0};
  bool pending_down[3]{false, false, false};
  double pending_wheel{0};
  std::string pending_typed;
  std::unordered_map<std::string, bool> pending_keys;
#ifdef _WIN32
  HWND hwnd{nullptr};
  bool timer_period{false};
#endif
};

WindowState g_window;

[[nodiscard]] std::string canonical_key(std::string_view name) {
  std::string key;
  for (const char c : name) {
    if (c != ' ' && c != '_' && c != '-') {
      key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
  }
  if (key == "return") {
    return "enter";
  }
  if (key == "esc") {
    return "escape";
  }
  if (key == "control") {
    return "ctrl";
  }
  if (key == "arrowleft") {
    return "left";
  }
  if (key == "arrowright") {
    return "right";
  }
  if (key == "arrowup") {
    return "up";
  }
  if (key == "arrowdown") {
    return "down";
  }
  return key;
}

[[nodiscard]] long long headless_frames(const char* value) {
  if (value == nullptr) {
    return 60;
  }
  char* end = nullptr;
  const long long frames = std::strtoll(value, &end, 10);
  return frames > 0 ? frames : 60;
}

#ifdef _WIN32

[[nodiscard]] std::string key_name(const WPARAM vk) {
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
    return std::string(1, static_cast<char>(std::tolower(static_cast<int>(vk))));
  }
  if (vk >= VK_F1 && vk <= VK_F12) {
    return "f" + std::to_string(vk - VK_F1 + 1);
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    return std::string(1, static_cast<char>('0' + (vk - VK_NUMPAD0)));
  }
  switch (vk) {
    case VK_SPACE:
      return "space";
    case VK_RETURN:
      return "enter";
    case VK_ESCAPE:
      return "escape";
    case VK_TAB:
      return "tab";
    case VK_BACK:
      return "backspace";
    case VK_DELETE:
      return "delete";
    case VK_LEFT:
      return "left";
    case VK_RIGHT:
      return "right";
    case VK_UP:
      return "up";
    case VK_DOWN:
      return "down";
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
      return "shift";
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
      return "ctrl";
    case VK_MENU:
      return "alt";
    case VK_HOME:
      return "home";
    case VK_END:
      return "end";
    case VK_PRIOR:
      return "pageup";
    case VK_NEXT:
      return "pagedown";
    case VK_INSERT:
      return "insert";
    default:
      return {};
  }
}

[[nodiscard]] std::wstring widen(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
  return wide;
}

void append_utf8(std::string& out, const std::uint32_t code) {
  if (code < 0x80) {
    out.push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out.push_back(static_cast<char>(0xC0U | (code >> 6U)));
    out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
  } else if (code < 0x10000) {
    out.push_back(static_cast<char>(0xE0U | (code >> 12U)));
    out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
  } else {
    out.push_back(static_cast<char>(0xF0U | (code >> 18U)));
    out.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
  }
}

void present(HDC dc) {
  const Canvas& surface = canvas();
  if (surface.width <= 0 || surface.height <= 0) {
    return;
  }
  RECT client{};
  GetClientRect(g_window.hwnd, &client);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = surface.width;
  info.bmiHeader.biHeight = -surface.height;  // top-down rows
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  StretchDIBits(dc, 0, 0, client.right - client.left, client.bottom - client.top, 0, 0, surface.width, surface.height,
                surface.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
}

std::uint32_t g_high_surrogate = 0;

LRESULT CALLBACK window_proc(HWND hwnd, const UINT message, const WPARAM wparam, const LPARAM lparam) {
  switch (message) {
    case WM_CLOSE:
      g_window.closed = true;
      return 0;
    case WM_SIZE: {
      const int width = LOWORD(lparam);
      const int height = HIWORD(lparam);
      if (width > 0 && height > 0 && (width != canvas().width || height != canvas().height)) {
        resize_canvas(width, height);  // the program redraws the next frame at the new size
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(hwnd, &paint);
      present(dc);
      EndPaint(hwnd, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_MOUSEMOVE:
      g_window.pending_x = GET_X_LPARAM(lparam);
      g_window.pending_y = GET_Y_LPARAM(lparam);
      return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      const bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
      const int button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)   ? 0
                         : (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? 1
                                                                                   : 2;
      g_window.pending_down[button] = down;
      g_window.pending_x = GET_X_LPARAM(lparam);
      g_window.pending_y = GET_Y_LPARAM(lparam);
      if (down) {
        SetCapture(hwnd);
      } else {
        ReleaseCapture();
      }
      return 0;
    }
    case WM_MOUSEWHEEL:
      g_window.pending_wheel += static_cast<double>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
      return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      const std::string name = key_name(wparam);
      if (!name.empty()) {
        g_window.pending_keys[name] = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      }
      if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) {
        break;  // keep Alt+F4 and the system menu working
      }
      return 0;
    }
    case WM_CHAR: {
      const auto unit = static_cast<std::uint32_t>(wparam);
      if (unit >= 0xD800 && unit <= 0xDBFF) {
        g_high_surrogate = unit;
        return 0;
      }
      std::uint32_t code = unit;
      if (unit >= 0xDC00 && unit <= 0xDFFF && g_high_surrogate != 0) {
        code = 0x10000U + ((g_high_surrogate - 0xD800U) << 10U) + (unit - 0xDC00U);
        g_high_surrogate = 0;
      }
      if (code >= 0x20 && code != 0x7F) {
        append_utf8(g_window.pending_typed, code);
      }
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool open_native(const std::string& title, const int width, const int height, std::string& error) {
  static bool registered = false;
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!registered) {
    SetProcessDPIAware();
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.lpszClassName = L"ClppWindow";
    if (RegisterClassExW(&window_class) == 0) {
      return fail(error, "Window.Open: cannot register the window class");
    }
    registered = true;
  }
  RECT frame{0, 0, width, height};
  AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
  const std::wstring wide_title = widen(title);
  g_window.hwnd = CreateWindowExW(0, L"ClppWindow", wide_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, instance, nullptr);
  if (g_window.hwnd == nullptr) {
    return fail(error, "Window.Open: cannot create the window");
  }
  ShowWindow(g_window.hwnd, SW_SHOW);
  UpdateWindow(g_window.hwnd);
  if (!g_window.timer_period) {
    g_window.timer_period = timeBeginPeriod(1) == TIMERR_NOERROR;  // precise Sleep for frame pacing
  }
  return true;
}

void pump_messages() {
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
}

#endif  // _WIN32

void close_window() {
#ifdef _WIN32
  if (g_window.hwnd != nullptr) {
    DestroyWindow(g_window.hwnd);
    g_window.hwnd = nullptr;
    pump_messages();
  }
#endif
  g_window.open = false;
  g_window.closed = true;
}

// Input that arrived since the last frame becomes this frame's state.
void apply_pending_input() {
  Input& in = g_window.input;
  for (int button = 0; button < 3; ++button) {
    in.was_down[button] = in.down[button];
    in.down[button] = g_window.pending_down[button];
  }
  in.mouse_x = g_window.pending_x;
  in.mouse_y = g_window.pending_y;
  in.wheel = g_window.pending_wheel;
  g_window.pending_wheel = 0;
  in.typed = std::move(g_window.pending_typed);
  g_window.pending_typed.clear();
  g_window.keys_before = g_window.keys;
  for (const auto& [name, down] : g_window.pending_keys) {
    g_window.keys[name] = down;
  }
  in.delta = g_window.delta;
  ++in.frame;
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(window_open) {
  if (arity != 3 || !args[0].is_string() || !args[1].is_number() || !args[2].is_number()) {
    return fail(error, "Window.Open: expected (string title, int width, int height)");
  }
  const int width = static_cast<int>(args[1].number);
  const int height = static_cast<int>(args[2].number);
  if (width < 1 || height < 1 || width > 8192 || height > 8192) {
    return fail(error, "Window.Open: size must be between 1 and 8192");
  }
  if (g_window.open) {
    close_window();
  }
  const char* headless = std::getenv("CLPP_HEADLESS");
  g_window = WindowState{};
  g_window.title = args[0].text;
  g_window.headless = headless != nullptr;
  g_window.frame_limit = headless_frames(headless);
  resize_canvas(width, height);
#ifdef _WIN32
  if (!g_window.headless && !open_native(g_window.title, width, height, error)) {
    return false;
  }
#else
  if (!g_window.headless) {
    static bool warned = false;
    if (!warned) {
      std::cerr << "warning: @clpp.window has no native backend on this system yet; running headless ("
                << g_window.frame_limit << " frames)\n";
      warned = true;
    }
    g_window.headless = true;
  }
#endif
  g_window.open = true;
  g_window.started = Clock::now();
  g_window.last_frame = g_window.started;
  out = boolean(true);
  return true;
}

CLPP_NATIVE(window_frame) {
  (void)args;
  (void)arity;
  (void)error;
  if (!g_window.open || g_window.closed) {
    out = boolean(false);
    return true;
  }
  const double target = 1.0 / std::max(1, g_window.fps);
  if (g_window.headless) {
    if (g_window.input.frame >= g_window.frame_limit) {
      g_window.open = false;
      out = boolean(false);
      return true;
    }
    g_window.delta = g_window.input.frame == 0 ? 0 : target;
    g_window.time += g_window.delta;
    g_window.measured_fps = g_window.fps;
  } else {
#ifdef _WIN32
    const auto now = Clock::now();
    if (g_window.input.frame > 0) {
      g_window.delta = std::chrono::duration<double>(now - g_window.last_frame).count();
      // Pace using the full period since the last Frame() return (includes the program's draw work).
      const auto deadline =
          g_window.last_frame + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(target));
      for (;;) {
        pump_messages();
        if (Clock::now() >= deadline || g_window.closed) {
          break;
        }
        const auto left = std::chrono::duration<double, std::milli>(deadline - Clock::now()).count();
        if (left > 2.0) {
          Sleep(static_cast<DWORD>(left - 1.0));
        } else {
          std::this_thread::yield();
        }
      }
      if (HDC dc = GetDC(g_window.hwnd)) {
        present(dc);
        ReleaseDC(g_window.hwnd, dc);
      }
      pump_messages();
    } else {
      g_window.delta = 0;
    }
    if (g_window.closed) {
      close_window();
      out = boolean(false);
      return true;
    }
#endif
  }
  apply_pending_input();
  if (!g_window.headless) {
#ifdef _WIN32
    const auto tick = Clock::now();
    g_window.time = std::chrono::duration<double>(tick - g_window.started).count();
    if (g_window.delta > 0) {
      g_window.measured_fps = g_window.measured_fps == 0 ? 1.0 / g_window.delta
                                                         : g_window.measured_fps * 0.9 + (1.0 / g_window.delta) * 0.1;
    }
    g_window.last_frame = tick;
#endif
  }
  out = boolean(true);
  return true;
}

CLPP_NATIVE(window_close) {
  (void)args;
  (void)arity;
  (void)out;
  (void)error;
  if (g_window.open) {
    close_window();
  }
  return true;
}

CLPP_NATIVE(window_query) {  // one native for the simple getters: (which)
  (void)arity;
  (void)error;
  const int which = static_cast<int>(num(args[0]));
  double value = 0;
  switch (which) {
    case 0:
      value = g_window.open && !g_window.closed ? 1 : 0;
      break;
    case 1:
      value = g_window.headless ? 1 : 0;
      break;
    case 2:
      value = canvas().width;
      break;
    case 3:
      value = canvas().height;
      break;
    case 4:
      value = g_window.time;
      break;
    case 5:
      value = g_window.delta;
      break;
    case 6:
      value = g_window.measured_fps;
      break;
    case 7:
      value = static_cast<double>(g_window.input.frame);
      break;
    case 8:
      value = g_window.input.mouse_x;
      break;
    case 9:
      value = g_window.input.mouse_y;
      break;
    case 10:
      value = g_window.input.wheel;
      break;
    default:
      break;
  }
  out = Value::number_of(value);
  return true;
}

CLPP_NATIVE(window_mouse) {  // (button, 0 down | 1 pressed | 2 released)
  if (!numbers(args, arity, error, "Window mouse")) {
    return false;
  }
  const int button = static_cast<int>(args[0].number);
  if (button < 0 || button > 2) {
    return fail(error, "mouse button must be 0 (left), 1 (right) or 2 (middle)");
  }
  const Input& in = g_window.input;
  const int mode = static_cast<int>(args[1].number);
  const bool result = mode == 0 ? in.down[button]
                      : mode == 1 ? in.down[button] && !in.was_down[button]
                                  : !in.down[button] && in.was_down[button];
  out = boolean(result);
  return true;
}

CLPP_NATIVE(window_key) {  // (name, 0 down | 1 pressed | 2 released)
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Window key: expected a key name such as \"Space\", \"A\" or \"Left\"");
  }
  const std::string name = canonical_key(args[0].text);
  const auto now = g_window.keys.find(name);
  const auto before = g_window.keys_before.find(name);
  const bool down = now != g_window.keys.end() && now->second;
  const bool was = before != g_window.keys_before.end() && before->second;
  const int mode = static_cast<int>(args[1].number);
  out = boolean(mode == 0 ? down : mode == 1 ? down && !was : !down && was);
  return true;
}

CLPP_NATIVE(window_typed) {
  (void)args;
  (void)arity;
  (void)error;
  out = Value::string_of(g_window.input.typed);
  return true;
}

CLPP_NATIVE(window_set_title) {
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Window.SetTitle: expected a string");
  }
  g_window.title = args[0].text;
#ifdef _WIN32
  if (g_window.hwnd != nullptr) {
    SetWindowTextW(g_window.hwnd, widen(g_window.title).c_str());
  }
#endif
  return true;
}

CLPP_NATIVE(window_set_fps) {
  (void)out;
  if (!numbers(args, arity, error, "Window.SetFps")) {
    return false;
  }
  g_window.fps = std::clamp(static_cast<int>(args[0].number), 1, 1000);
  return true;
}

CLPP_NATIVE(window_dialog) {  // (title, message, 0 alert | 1 confirm)
  if (arity != 3 || !args[0].is_string() || !args[1].is_string()) {
    return fail(error, "Window.Alert/Confirm: expected (string title, string message)");
  }
  const bool confirm = num(args[2]) != 0;
  if (g_window.headless || std::getenv("CLPP_HEADLESS") != nullptr) {
    out = boolean(false);
    return true;
  }
#ifdef _WIN32
  const int answer = MessageBoxW(g_window.hwnd, widen(args[1].text).c_str(), widen(args[0].text).c_str(),
                                 confirm ? (MB_YESNO | MB_ICONQUESTION) : (MB_OK | MB_ICONINFORMATION));
  out = boolean(confirm && answer == IDYES);
#else
  std::cerr << args[0].text << ": " << args[1].text << '\n';
  out = boolean(false);
#endif
  return true;
}

CLPP_NATIVE(window_simulate_mouse) {  // (x, y, down)
  (void)out;
  if (!numbers(args, arity, error, "Window.SimulateMouse")) {
    return false;
  }
  g_window.pending_x = args[0].number;
  g_window.pending_y = args[1].number;
  g_window.pending_down[0] = args[2].number != 0;
  return true;
}

CLPP_NATIVE(window_simulate_key) {  // (name, down)
  (void)out;
  if (arity != 2 || !args[0].is_string() || !args[1].is_number()) {
    return fail(error, "Window.SimulateKey: expected (string key, bool down)");
  }
  g_window.pending_keys[canonical_key(args[0].text)] = args[1].number != 0;
  return true;
}

CLPP_NATIVE(window_simulate_text) {
  (void)out;
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Window.SimulateText: expected a string");
  }
  g_window.pending_typed += args[0].text;
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kWindowSource = R"clp(<< @clpp.window: a native window that shows the @clpp.gfx canvas, with input and timing.
<< Headless (no window) when CLPP_HEADLESS is set or the system has no backend: Frame() then
<< returns false after CLPP_HEADLESS frames (default 60).

const LEFT = 0;
const RIGHT = 1;
const MIDDLE = 2;

func Open(string title, int width, int height) -> bool { return window::Open(title, width, height); }
func Frame() -> bool { return window::Frame(); }
func Close() { window::Close(); }
func IsOpen() -> bool { return window::Query(0); }
func IsHeadless() -> bool { return window::Query(1); }
func Width() -> int { return window::Query(2); }
func Height() -> int { return window::Query(3); }
func Time() -> float { return window::Query(4); }
func Delta() -> float { return window::Query(5); }
func Fps() -> float { return window::Query(6); }
func FrameCount() -> int { return window::Query(7); }
func MouseX() -> float { return window::Query(8); }
func MouseY() -> float { return window::Query(9); }
func Wheel() -> float { return window::Query(10); }
func MouseDown(int button) -> bool { return window::Mouse(button, 0); }
func MousePressed(int button) -> bool { return window::Mouse(button, 1); }
func MouseReleased(int button) -> bool { return window::Mouse(button, 2); }
func KeyDown(string key) -> bool { return window::Key(key, 0); }
func KeyPressed(string key) -> bool { return window::Key(key, 1); }
func KeyReleased(string key) -> bool { return window::Key(key, 2); }
func TypedText() -> string { return window::Typed(); }
func SetTitle(string title) { window::SetTitle(title); }
func SetFps(int fps) { window::SetFps(fps); }
func Alert(string title, string message) { window::Dialog(title, message, 0); }
func Confirm(string title, string message) -> bool { return window::Dialog(title, message, 1); }
func SimulateMouse(float x, float y, bool down) { window::SimulateMouse(x, y, down); }
func SimulateKey(string key, bool down) { window::SimulateKey(key, down); }
func SimulateText(string text) { window::SimulateText(text); }
)clp";

}  // namespace

const Input& input() { return g_window.input; }

bool key_down(const std::string_view name) {
  const auto found = g_window.keys.find(canonical_key(name));
  return found != g_window.keys.end() && found->second;
}

bool key_pressed(const std::string_view name) {
  const std::string key = canonical_key(name);
  const auto now = g_window.keys.find(key);
  const auto before = g_window.keys_before.find(key);
  return now != g_window.keys.end() && now->second && !(before != g_window.keys_before.end() && before->second);
}

void add_window(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"window::Open", 3, window_open, true},
                                {"window::Frame", 0, window_frame, true},
                                {"window::Close", 0, window_close, true},
                                {"window::Query", 1, window_query, true},
                                {"window::Mouse", 2, window_mouse, true},
                                {"window::Key", 2, window_key, true},
                                {"window::Typed", 0, window_typed, true},
                                {"window::SetTitle", 1, window_set_title, true},
                                {"window::SetFps", 1, window_set_fps, true},
                                {"window::Dialog", 3, window_dialog, true},
                                {"window::SimulateMouse", 3, window_simulate_mouse, true},
                                {"window::SimulateKey", 2, window_simulate_key, true},
                                {"window::SimulateText", 1, window_simulate_text, true},
                            });
}

std::string_view window_source() { return kWindowSource; }

}  // namespace clpp::stdlib::host
