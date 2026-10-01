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
        "func abs(n) {\n"
        "  if (n < 0) {\n"
        "    return 0 - n;\n"
        "  }\n"
        "  return n;\n"
        "}\n");
  }
  if (path == "@clpp.text") {
    return std::string(
        "func trim(string text) -> string { return string::trim(text); }\n"
        "func lower(string text) -> string { return string::lower(text); }\n"
        "func upper(string text) -> string { return string::upper(text); }\n"
        "func contains(string text, string part) -> bool { return string::contains(text, part); }\n"
        "func split(string text, string separator) { return string::split(text, separator); }\n"
        "func replace(string text, string target, string replacement) -> string { return string::replace(text, target, replacement); }\n"
        "func startsWith(string text, string part) -> bool { return string::starts_with(text, part); }\n"
        "func endsWith(string text, string part) -> bool { return string::ends_with(text, part); }\n"
        "func repeat(string text, int count) -> string { return string::repeat(text, count); }\n"
        "func toNumber(string text) -> float { return string::to_number(text); }\n"
        "func indexOf(string text, string part) -> int { return string::index_of(text, part); }\n"
        "func joinAll(parts, string separator) -> string { return string::join_list(parts, separator); }\n");
  }
  if (path == "@clpp.os") {
    return std::string(
        "func env(string name) -> string {\n"
        "  return os::env(name);\n"
        "}\n");
  }
  if (path == "@clpp.http") {
    return std::string(
        "func host(string url) -> string {\n"
        "  return http::host(url);\n"
        "}\n");
  }
  return std::nullopt;
}

}  // namespace clpp::stdlib
