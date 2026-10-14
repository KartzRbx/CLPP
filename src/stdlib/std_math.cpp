#include "clpp/stdlib.hpp"
#include "stdlib/host.hpp"

namespace clpp::stdlib {

const std::vector<ModuleEntry>& module_catalog() {
  static const std::vector<ModuleEntry> catalog = {
      {"@clpp.axiom", "Axiom", "Game math: Clamp, Lerp, Smoothstep, vectors, angles, easing, noise, colors"},
      {"@clpp.window", "Window", "Native window: frame loop, keyboard, mouse, timing, dialogs, input simulation"},
      {"@clpp.gfx", "Gfx", "2D drawing: shapes, text, gradients, images (PNG/BMP), save to PNG"},
      {"@clpp.gui", "Gui", "Declarative UI (Roblox style): Screen, Frame, Row, Text, Button, Toggle, Slider, TextBox, Image; Clicked/Changed"},
      {"@clpp.ui", "Ui", "Immediate-mode UI: panels, buttons, checkboxes, sliders, inputs, choices, themes"},
      {"@clpp.audio", "Audio", "Sound: WAV files, synthesized tones and sweeps, volume, pan, pitch, loops"},
      {"@clpp.json", "Json", "JSON: Parse, Stringify, Pretty, Get/Has/Set by path"},
      {"@clpp.time", "Time", "Clocks, Sleep, dates and Format"},
      {"@clpp.input", "Input", "Desktop automation (AutoHotkey style): move/click mouse, press keys, hotkeys, type text, read screen"},
      {"@clpp.io", "Io", "Console: Print without newline, ReadLine, ReadKey, ReadNumber, colors, cursor, Clear"},
      {"@clpp.text", "Text", "Text: trim, lower, upper, contains, split, replace, startsWith, endsWith, repeat, toNumber, indexOf, joinAll"},
      {"@clpp.fs", "Fs", "Files: read, write, append, exists, remove, list, makeDir, size"},
      {"@clpp.math", "Math", "Basic math: abs"},
      {"@clpp.os", "Os", "Process: env"},
      {"@clpp.http", "Http", "URLs: host"},
  };
  return catalog;
}

std::optional<std::string> module_source(const std::string_view path) {
  if (path == "@clpp.axiom" || path == "@clpp.Axion" || path == "@clpp.libs.axiom" || path == "@clpp.libs.Axion" ||
      path == "@clpp.mathutils") {
    return std::string(axiom_source());
  }
  if (path == "@clpp.window") {
    return std::string(host::window_source());
  }
  if (path == "@clpp.gfx") {
    return std::string(host::gfx_source());
  }
  if (path == "@clpp.gui") {
    return std::string(host::gui_source());
  }
  if (path == "@clpp.ui") {
    return std::string(host::ui_source());
  }
  if (path == "@clpp.audio") {
    return std::string(host::audio_source());
  }
  if (path == "@clpp.json") {
    return std::string(host::json_source());
  }
  if (path == "@clpp.time") {
    return std::string(host::time_source());
  }
  if (path == "@clpp.input") {
    return std::string(host::input_source());
  }
  if (path == "@clpp.io") {
    return std::string(host::io_source());
  }
  if (path == "@clpp.fs") {
    return std::string(host::fs_source());
  }
  if (path == "@clpp.math") {
    return std::string(
        "export any abs(any n) {\n"
        "  if (n < 0) {\n"
        "    return 0 - n;\n"
        "  }\n"
        "  return n;\n"
        "}\n");
  }
  if (path == "@clpp.text") {
    return std::string(
        "export string trim(string text) { return string::trim(text); }\n"
        "export string lower(string text) { return string::lower(text); }\n"
        "export string upper(string text) { return string::upper(text); }\n"
        "export bool contains(string text, string part) { return string::contains(text, part); }\n"
        "export array<string> split(string text, string separator) { return string::split(text, separator); }\n"
        "export string replace(string text, string target, string replacement) { return string::replace(text, target, replacement); }\n"
        "export bool startsWith(string text, string part) { return string::starts_with(text, part); }\n"
        "export bool endsWith(string text, string part) { return string::ends_with(text, part); }\n"
        "export string repeat(string text, number count) { return string::repeat(text, count); }\n"
        "export float toNumber(string text) { return string::to_number(text); }\n"
        "export number indexOf(string text, string part) { return string::index_of(text, part); }\n"
        "export string joinAll(array<string> parts, string separator) { return string::join_list(parts, separator); }\n");
  }
  if (path == "@clpp.os") {
    return std::string(
        "export string env(string name) {\n"
        "  return os::env(name);\n"
        "}\n");
  }
  if (path == "@clpp.http") {
    return std::string(
        "export string host(string url) {\n"
        "  return http::host(url);\n"
        "}\n");
  }
  return std::nullopt;
}

}  // namespace clpp::stdlib
