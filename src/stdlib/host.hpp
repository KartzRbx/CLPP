#pragma once

// Host libraries: natives that keep state between calls or talk to the operating system.
//
// Each library (gfx, window, ui, json, time, fs) appends its natives to one registry. The id of a
// native is its position in that registry, so `window::Open(...)` in a module source compiles to
// the Host opcode with that id (binder: 2000 + id). Libraries also provide the CL++ source of
// their `@clpp.*` module: typed wrappers that the editor shows and the type checker uses.

#include "clpp/value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::stdlib::host {

using Native = bool (*)(const Value* args, std::uint8_t arity, Value& out, std::string& error);

struct Entry {
  std::string_view name;  // "window::Open"
  std::uint8_t arity;
  Native function;
  bool sandboxed;  // refused inside actor(...): files, windows, input
};

void add_gfx(std::vector<Entry>& table);
void add_window(std::vector<Entry>& table);
void add_ui(std::vector<Entry>& table);
void add_json(std::vector<Entry>& table);
void add_time(std::vector<Entry>& table);
void add_files(std::vector<Entry>& table);
void add_io(std::vector<Entry>& table);
void add_gui(std::vector<Entry>& table);
void add_audio(std::vector<Entry>& table);
void add_input(std::vector<Entry>& table);

[[nodiscard]] std::string_view gfx_source();
[[nodiscard]] std::string_view window_source();
[[nodiscard]] std::string_view ui_source();
[[nodiscard]] std::string_view json_source();
[[nodiscard]] std::string_view time_source();
[[nodiscard]] std::string_view fs_source();
[[nodiscard]] std::string_view io_source();
[[nodiscard]] std::string_view gui_source();
[[nodiscard]] std::string_view audio_source();
[[nodiscard]] std::string_view input_source();

// --- argument helpers -----------------------------------------------------------------------

[[nodiscard]] inline bool fail(std::string& error, std::string message) {
  error = std::move(message);
  return false;
}

[[nodiscard]] inline double num(const Value& value) { return value.is_number() ? value.number : 0.0; }

[[nodiscard]] inline bool numbers(const Value* args, const std::uint8_t arity, std::string& error,
                                  const std::string_view what) {
  for (std::uint8_t index = 0; index < arity; ++index) {
    if (!args[index].is_number()) {
      return fail(error, std::string(what) + ": argument " + std::to_string(index + 1) + " must be a number");
    }
  }
  return true;
}

[[nodiscard]] inline Value boolean(const bool value) { return Value::number_of(value ? 1.0 : 0.0); }

// --- shared drawing surface (gfx owns it; window presents it; ui draws on it) ----------------

struct Canvas {
  int width{0};
  int height{0};
  std::vector<std::uint32_t> pixels;  // 0xAARRGGBB, alpha always 255 after compositing
};

[[nodiscard]] Canvas& canvas();
void resize_canvas(int width, int height);

// Drawing primitives in C++ for the other host libraries (ui). Colors use the CL++ convention:
// 0xRRGGBB, with the top byte as transparency (0 = opaque, 255 = invisible).
void fill_rect(double x, double y, double w, double h, std::uint32_t color);
void fill_round_rect(double x, double y, double w, double h, double radius, std::uint32_t color);
void stroke_rect(double x, double y, double w, double h, double thickness, std::uint32_t color);
void fill_circle(double cx, double cy, double r, std::uint32_t color);
void stroke_line(double x1, double y1, double x2, double y2, double thickness, std::uint32_t color);
void stroke_round_rect(double x, double y, double w, double h, double radius, double thickness, std::uint32_t color);
// Soft drop shadow: a rounded rectangle whose edge fades out over `blur` pixels.
void fill_shadow(double x, double y, double w, double h, double radius, double blur, std::uint32_t color);
// Text: `size` is the font size in pixels, `y` the top of the line (see gfx_text.hpp).
void draw_text(std::string_view text, double x, double y, double size, std::uint32_t color);
[[nodiscard]] double text_width(std::string_view text, double size);
[[nodiscard]] double text_height(double size);
[[nodiscard]] std::uint32_t mix_color(std::uint32_t a, std::uint32_t b, double t);
void set_clip(double x, double y, double w, double h);
void clear_clip();
// Adds an image (0xAARRGGBB pixels) to @clpp.gfx and returns its handle.
[[nodiscard]] int add_image(int width, int height, std::vector<std::uint32_t> pixels);
// Loads a PNG/BMP file (cached by path); 0 and `error` on failure.
[[nodiscard]] int load_image_file(const std::string& path, std::string& error);
[[nodiscard]] bool image_dimensions(int handle, int& width, int& height);
void draw_image_box(int handle, double x, double y, double w, double h);
// Blurs the canvas inside the rectangle (frosted-glass panels).
void blur_area(double x, double y, double w, double h, double radius);
// Snapshot/restore of a canvas rectangle (fade-in animations blend the two).
[[nodiscard]] std::vector<std::uint32_t> read_area(int x, int y, int w, int h);
void blend_area(int x, int y, int w, int h, const std::vector<std::uint32_t>& before, double amount_new);

// --- input and frame state (window owns it; ui reads it) -------------------------------------

struct Input {
  double mouse_x{0};
  double mouse_y{0};
  bool down[3]{false, false, false};
  bool was_down[3]{false, false, false};
  double wheel{0};
  std::string typed;
  long long frame{0};
  double delta{0};  // seconds since the previous frame
};

[[nodiscard]] const Input& input();
[[nodiscard]] bool key_down(std::string_view name);
[[nodiscard]] bool key_pressed(std::string_view name);

}  // namespace clpp::stdlib::host
