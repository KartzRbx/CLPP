// CL++ language server (Language Server Protocol 3.17 over stdio).
//
// Design notes (following the VS Code "Language Server Extension Guide" and the way the TypeScript
// language service is built):
// - The server never stops at the first error: every request is answered from whatever the
//   error-tolerant front end could build, so completion works on a half-typed line.
// - JSON goes through a real JSON library; responses are always valid JSON-RPC.
// - Positions are UTF-16 on the wire and byte columns inside the compiler; every conversion
//   happens here, in one place.
// - Linked modules are read from the editor's open buffers first (unsaved edits count), then
//   from disk, so completion across files follows what the user sees.
// - Unknown requests get MethodNotFound instead of silence, so the client never waits.

#include "clpp/ide.hpp"
#include "clpp/semantic.hpp"
#include "clpp/stdlib.hpp"
#include "core/compiler/analyze.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace clpp::ide {

namespace {

using json = nlohmann::json;

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInternalError = -32603;
constexpr int kServerNotInitialized = -32002;

// ---------------------------------------------------------------------------------------------
// URIs and paths
// ---------------------------------------------------------------------------------------------

[[nodiscard]] int hex_value(const char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}

// file:///c%3A/Users/x/a.clp -> c:/Users/x/a.clp (Windows) ; file:///home/x/a.clp -> /home/x/a.clp
[[nodiscard]] std::filesystem::path uri_to_path(const std::string_view uri) {
  std::string_view rest = uri;
  if (rest.rfind("file://", 0) == 0) {
    rest.remove_prefix(7);
  }
  std::string decoded;
  for (std::size_t index = 0; index < rest.size(); ++index) {
    if (rest[index] == '%' && index + 2 < rest.size()) {
      decoded.push_back(static_cast<char>(hex_value(rest[index + 1]) * 16 + hex_value(rest[index + 2])));
      index += 2;
    } else {
      decoded.push_back(rest[index]);
    }
  }
  // "/c:/x" -> "c:/x"
  if (decoded.size() >= 3 && decoded[0] == '/' && decoded[2] == ':') {
    decoded.erase(0, 1);
  }
  return std::filesystem::path(decoded).lexically_normal();
}

[[nodiscard]] std::string path_to_uri(const std::filesystem::path& path) {
  std::string text = path.lexically_normal().generic_string();
  std::string encoded = "file://";
  if (!text.empty() && text.front() != '/') {
    encoded.push_back('/');  // Windows drive: file:///c:/...
  }
  static constexpr char kHex[] = "0123456789ABCDEF";
  for (const unsigned char c : text) {
    const bool plain = (std::isalnum(c) != 0) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':';
    if (plain) {
      encoded.push_back(static_cast<char>(c));
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[c >> 4]);
      encoded.push_back(kHex[c & 15]);
    }
  }
  return encoded;
}

[[nodiscard]] std::string key_of(const std::filesystem::path& path) {
  std::string key = path.lexically_normal().generic_string();
#ifdef _WIN32
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
  return key;
}

// ---------------------------------------------------------------------------------------------
// Server state
// ---------------------------------------------------------------------------------------------

struct Document {
  std::string uri;
  std::string text;
  int version{0};
};

class Server {
 public:
  Server(std::istream& in, std::ostream& out) : m_in(in), m_out(out) {}

  int run() {
    while (true) {
      std::string body;
      if (!read_message(body)) {
        return m_shutdown ? 0 : 1;
      }
      json message;
      try {
        message = json::parse(body);
      } catch (const std::exception&) {
        send_error(nullptr, kParseError, "invalid JSON");
        continue;
      }
      if (!message.is_object()) {
        send_error(nullptr, kInvalidRequest, "expected an object");
        continue;
      }
      const json id = message.contains("id") ? message["id"] : json();
      const std::string method = message.value("method", "");
      const json params = message.contains("params") ? message["params"] : json::object();
      if (method.empty()) {
        continue;  // a response to something we never send
      }
      if (method == "exit") {
        return m_shutdown ? 0 : 1;
      }
      try {
        dispatch(method, id, params);
      } catch (const std::exception& error) {
        if (!id.is_null()) {
          send_error(id, kInternalError, error.what());
        }
      }
    }
  }

 private:
  // --- transport -----------------------------------------------------------------------------

  bool read_message(std::string& body) {
    std::size_t length = 0;
    bool any_header = false;
    std::string line;
    while (std::getline(m_in, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line.empty()) {
        if (any_header) {
          break;
        }
        continue;
      }
      any_header = true;
      const std::size_t colon = line.find(':');
      if (colon == std::string::npos) {
        continue;
      }
      std::string name = line.substr(0, colon);
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (name == "content-length") {
        try {
          length = static_cast<std::size_t>(std::stoul(line.substr(colon + 1)));
        } catch (...) {
          length = 0;
        }
      }
    }
    if (!m_in || length == 0) {
      return false;
    }
    body.assign(length, '\0');
    m_in.read(body.data(), static_cast<std::streamsize>(length));
    return m_in.gcount() == static_cast<std::streamsize>(length);
  }

  void send(const json& message) {
    const std::string body = message.dump(-1, ' ', false, json::error_handler_t::replace);
    m_out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    m_out.flush();
  }

  void reply(const json& id, json result) { send(json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}); }

  void send_error(const json& id, const int code, const std::string& message) {
    send(json{{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}});
  }

  void notify(const std::string& method, json params) {
    send(json{{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
  }

  // --- documents and modules -----------------------------------------------------------------

  [[nodiscard]] ModuleLoader loader_for(const std::string& uri) const {
    const std::filesystem::path base = uri_to_path(uri).parent_path();
    return [this, base](const std::string_view path) -> std::optional<std::string> {
      const std::filesystem::path target = (base / std::filesystem::path(std::string(path))).lexically_normal();
      const auto open = m_documents.find(key_of(target));
      if (open != m_documents.end()) {
        return open->second.text;  // unsaved edits in another tab win over the disk
      }
      std::ifstream file(target, std::ios::binary);
      if (!file) {
        return std::nullopt;
      }
      return std::string(std::istreambuf_iterator<char>(file), {});
    };
  }

  [[nodiscard]] Document* document(const json& params) {
    const std::string uri = params.at("textDocument").at("uri").get<std::string>();
    const auto found = m_documents.find(key_of(uri_to_path(uri)));
    return found == m_documents.end() ? nullptr : &found->second;
  }

  // --- conversions ---------------------------------------------------------------------------

  // LSP (0-based line, UTF-16 character) -> compiler (1-based line, 1-based byte column)
  [[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> to_compiler(const std::string& text, const json& position) {
    const int line = position.at("line").get<int>();
    const int character = position.at("character").get<int>();
    const int bytes = byte_column_from_utf16(text, line, character);
    return {static_cast<std::uint32_t>(line + 1), static_cast<std::uint32_t>(bytes + 1)};
  }

  [[nodiscard]] static json position(const std::string& text, const std::uint32_t line1, const std::uint32_t column1) {
    const int line = line1 == 0 ? 0 : static_cast<int>(line1) - 1;
    return json{{"line", line}, {"character", lsp_character_at(text, static_cast<int>(line1), static_cast<int>(column1))}};
  }

  [[nodiscard]] static json range(const std::string& text, const std::uint32_t line1, const std::uint32_t column1,
                                  const std::uint32_t length_bytes) {
    return json{{"start", position(text, line1, column1)},
                {"end", position(text, line1, column1 + std::max<std::uint32_t>(length_bytes, 1))}};
  }

  [[nodiscard]] static int completion_kind(const Kind kind, const bool snippet) {
    if (snippet && kind == Kind::Keyword) {
      return 15;  // Snippet
    }
    switch (kind) {
      case Kind::Function:
        return 3;
      case Kind::Field:
        return 5;
      case Kind::Variable:
        return 6;
      case Kind::Module:
        return 9;
      case Kind::Enum:
        return 13;
      case Kind::Keyword:
        return 14;
      case Kind::Struct:
        return 22;
    }
    return 1;
  }

  [[nodiscard]] static int symbol_kind(const Kind kind) {
    switch (kind) {
      case Kind::Function:
        return 12;
      case Kind::Struct:
        return 23;
      case Kind::Enum:
        return 10;
      case Kind::Field:
        return 8;
      case Kind::Module:
        return 2;
      case Kind::Variable:
        return 13;
      case Kind::Keyword:
        return 14;
    }
    return 13;
  }

  // --- diagnostics ---------------------------------------------------------------------------

  void publish(const Document& doc) {
    const AnalysisResult analysis = analyze_program(doc.text, loader_for(doc.uri));
    const Info info = build(analysis, doc, 1, 1);
    json diagnostics = json::array();
    for (const Diagnostic& diagnostic : info.diagnostics) {
      const std::uint32_t line = std::max<std::uint32_t>(diagnostic.location.line, 1);
      const std::uint32_t column = std::max<std::uint32_t>(diagnostic.location.column, 1);
      json item{{"range", range(doc.text, line, column, static_cast<std::uint32_t>(diagnostic.span.size()))},
                {"severity", 1},
                {"source", "clpp"},
                {"message", diagnostic.message}};
      diagnostics.push_back(std::move(item));
    }
    notify("textDocument/publishDiagnostics",
           json{{"uri", doc.uri}, {"version", doc.version}, {"diagnostics", std::move(diagnostics)}});
  }

  void publish_all() {
    for (const auto& [key, doc] : m_documents) {
      publish(doc);
    }
  }

  [[nodiscard]] static Info build(const AnalysisResult& analysis, const Document& doc, const std::uint32_t line,
                                  const std::uint32_t column) {
    return build_info_for(analysis, doc.text, line, column);
  }

  // --- requests ------------------------------------------------------------------------------

  void dispatch(const std::string& method, const json& id, const json& params) {
    const bool request = !id.is_null();
    if (method == "initialize") {
      m_initialized = true;
      reply(id, initialize_result());
      return;
    }
    if (!m_initialized) {
      if (request) {
        send_error(id, kServerNotInitialized, "server not initialized");
      }
      return;
    }
    if (method == "initialized" || method.rfind("$/", 0) == 0 || method == "workspace/didChangeConfiguration" ||
        method == "workspace/didChangeWatchedFiles") {
      if (method == "workspace/didChangeWatchedFiles") {
        publish_all();  // a linked file changed on disk
      }
      if (request && method.rfind("$/", 0) == 0) {
        send_error(id, kMethodNotFound, "unsupported: " + method);
      }
      return;
    }
    if (method == "shutdown") {
      m_shutdown = true;
      reply(id, nullptr);
      return;
    }
    if (method == "textDocument/didOpen") {
      const json& text_document = params.at("textDocument");
      Document doc;
      doc.uri = text_document.at("uri").get<std::string>();
      doc.text = text_document.at("text").get<std::string>();
      doc.version = text_document.value("version", 0);
      m_documents[key_of(uri_to_path(doc.uri))] = doc;
      publish_all();  // files that link this one may change too
      return;
    }
    if (method == "textDocument/didChange") {
      Document* doc = document(params);
      if (doc == nullptr) {
        return;
      }
      for (const json& change : params.at("contentChanges")) {
        if (!change.contains("range")) {
          doc->text = change.at("text").get<std::string>();
          continue;
        }
        // Incremental edit (in case a client sends one).
        const auto [start_line, start_column] = to_compiler(doc->text, change.at("range").at("start"));
        const auto [end_line, end_column] = to_compiler(doc->text, change.at("range").at("end"));
        const auto offset = [&](const std::uint32_t line, const std::uint32_t column) {
          std::size_t index = 0;
          std::uint32_t current = 1;
          while (index < doc->text.size() && current < line) {
            if (doc->text[index++] == '\n') {
              ++current;
            }
          }
          return std::min(doc->text.size(), index + column - 1);
        };
        const std::size_t from = offset(start_line, start_column);
        const std::size_t to = offset(end_line, end_column);
        doc->text.replace(from, to - from, change.at("text").get<std::string>());
      }
      doc->version = params.at("textDocument").value("version", doc->version);
      publish_all();
      return;
    }
    if (method == "textDocument/didSave") {
      publish_all();
      return;
    }
    if (method == "textDocument/didClose") {
      const std::string uri = params.at("textDocument").at("uri").get<std::string>();
      m_documents.erase(key_of(uri_to_path(uri)));
      notify("textDocument/publishDiagnostics", json{{"uri", uri}, {"diagnostics", json::array()}});
      return;
    }
    if (!request) {
      return;  // unknown notification
    }

    if (method == "textDocument/completion" || method == "textDocument/hover" || method == "textDocument/definition" ||
        method == "textDocument/signatureHelp" || method == "textDocument/documentSymbol" ||
        method == "textDocument/references" || method == "textDocument/rename" ||
        method == "textDocument/prepareRename" || method == "textDocument/semanticTokens/full" ||
        method == "textDocument/formatting" || method == "textDocument/foldingRange" ||
        method == "textDocument/codeAction" || method == "textDocument/documentHighlight") {
      Document* doc = document(params);
      if (doc == nullptr) {
        reply(id, nullptr);
        return;
      }
      handle_document_request(method, id, params, *doc);
      return;
    }
    send_error(id, kMethodNotFound, "unsupported: " + method);
  }

  [[nodiscard]] static json initialize_result() {
    json token_types = json::array({"type", "struct", "enum", "enumMember", "parameter", "variable", "property",
                                    "function", "keyword"});
    json token_modifiers = json::array({"declaration", "readonly", "async"});
    return json{
        {"capabilities",
         {{"positionEncoding", "utf-16"},
          {"textDocumentSync", {{"openClose", true}, {"change", 1}, {"save", {{"includeText", false}}}}},
          {"completionProvider",
           {{"triggerCharacters", json::array({".", "@", "\"", "/", ":"})}, {"resolveProvider", false}}},
          {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}, {"retriggerCharacters", json::array({","})}}},
          {"hoverProvider", true},
          {"definitionProvider", true},
          {"referencesProvider", true},
          {"documentHighlightProvider", true},
          {"renameProvider", {{"prepareProvider", true}}},
          {"foldingRangeProvider", true},
          {"codeActionProvider", {{"codeActionKinds", json::array({"quickfix"})}}},
          {"documentFormattingProvider", true},
          {"documentSymbolProvider", true},
          {"semanticTokensProvider",
           {{"legend", {{"tokenTypes", token_types}, {"tokenModifiers", token_modifiers}}}, {"full", true}}}}},
        {"serverInfo", {{"name", "clpp"}, {"version", "0.9"}}}};
  }

  void handle_document_request(const std::string& method, const json& id, const json& params, Document& doc) {
    const std::string& text = doc.text;
    const AnalysisResult analysis = analyze_program(text, loader_for(doc.uri));

    if (method == "textDocument/semanticTokens/full") {
      const std::vector<std::uint32_t> data = encode_semantic_tokens(semantic_tokens(text, loader_for(doc.uri)));
      reply(id, json{{"data", data}});
      return;
    }
    if (method == "textDocument/formatting") {
      const std::string formatted = format_source(text);
      int end_line = 0;
      for (const char c : text) {
        end_line += c == '\n' ? 1 : 0;
      }
      reply(id, json::array({json{{"range", {{"start", {{"line", 0}, {"character", 0}}}, {"end", {{"line", end_line + 1}, {"character", 0}}}}},
                                  {"newText", formatted}}}));
      return;
    }
    if (method == "textDocument/foldingRange") {
      json ranges = json::array();
      std::vector<int> open;
      int line = 0;
      for (const Token& token : analysis.tokens) {
        line = static_cast<int>(token.location.line) - 1;
        if (token.type == TokenType::OpenBrace) {
          open.push_back(line);
        } else if (token.type == TokenType::CloseBrace && !open.empty()) {
          const int start = open.back();
          open.pop_back();
          if (line > start) {
            ranges.push_back(json{{"startLine", start}, {"endLine", line - 1 >= start ? line - 1 : start}});
          }
        }
      }
      reply(id, ranges);
      return;
    }
    if (method == "textDocument/documentSymbol") {
      reply(id, document_symbols(analysis, text));
      return;
    }
    if (method == "textDocument/codeAction") {
      json actions = json::array();
      const json context = params.value("context", json::object());
      for (const json& diagnostic : context.value("diagnostics", json::array())) {
        if (diagnostic.value("message", "") != "cannot assign to immutable binding") {
          continue;
        }
        // Find the `let name` that declares the assigned binding and offer `let mut name`.
        const auto [line, column] = to_compiler(text, diagnostic.at("range").at("start"));
        std::string name;
        for (const Token& token : analysis.tokens) {
          if (token.location.line == line && token.location.column == column) {
            name = std::string(token.lexeme);
          }
        }
        for (std::size_t index = 0; index + 1 < analysis.tokens.size() && !name.empty(); ++index) {
          const Token& keyword = analysis.tokens[index];
          const Token& target = analysis.tokens[index + 1];
          if (keyword.type == TokenType::KwLet && target.lexeme == name) {
            json edit{{"range", range(text, target.location.line, target.location.column, 0)}, {"newText", "mut "}};
            edit["range"]["end"] = edit["range"]["start"];
            actions.push_back(json{{"title", "Tornar '" + name + "' mutável (let mut)"},
                                   {"kind", "quickfix"},
                                   {"diagnostics", json::array({diagnostic})},
                                   {"isPreferred", true},
                                   {"edit", {{"changes", {{doc.uri, json::array({edit})}}}}}});
            break;
          }
        }
      }
      reply(id, actions);
      return;
    }

    const auto [line, column] = to_compiler(text, params.at("position"));
    if (method == "textDocument/references" || method == "textDocument/rename" ||
        method == "textDocument/prepareRename" || method == "textDocument/documentHighlight") {
      const SemanticSnapshot snapshot = build_snapshot(analysis);
      const std::vector<SemanticSymbol> matches = references_at(snapshot, line, column);
      if (method == "textDocument/prepareRename") {
        if (matches.empty()) {
          reply(id, nullptr);
        } else {
          for (const SemanticSymbol& symbol : matches) {
            if (symbol.location.line == line && column >= symbol.location.column &&
                column <= symbol.location.column + symbol.name.size()) {
              reply(id, json{{"range", range(text, symbol.location.line, symbol.location.column,
                                              static_cast<std::uint32_t>(symbol.name.size()))},
                             {"placeholder", symbol.name}});
              return;
            }
          }
          reply(id, nullptr);
        }
        return;
      }
      json locations = json::array();
      json edits = json::array();
      json highlights = json::array();
      for (const SemanticSymbol& symbol : matches) {
        const json where = range(text, symbol.location.line, symbol.location.column,
                                 static_cast<std::uint32_t>(symbol.name.size()));
        locations.push_back(json{{"uri", doc.uri}, {"range", where}});
        highlights.push_back(json{{"range", where}, {"kind", symbol.declaration ? 3 : 2}});
        if (method == "textDocument/rename") {
          edits.push_back(json{{"range", where}, {"newText", params.at("newName").get<std::string>()}});
        }
      }
      if (method == "textDocument/rename") {
        reply(id, matches.empty() ? json(nullptr) : json{{"changes", {{doc.uri, edits}}}});
      } else if (method == "textDocument/documentHighlight") {
        reply(id, highlights);
      } else {
        reply(id, locations);
      }
      return;
    }

    const Info info = build(analysis, doc, line, column);
    if (method == "textDocument/completion") {
      json items = json::array();
      for (const Item& item : info.completions) {
        json entry{{"label", item.label},
                   {"kind", completion_kind(item.kind, item.snippet)},
                   {"detail", item.detail},
                   {"sortText", item.sort_text + item.label}};
        if (!item.documentation.empty()) {
          entry["documentation"] = json{{"kind", "markdown"}, {"value", item.documentation}};
        }
        if (!item.insert_text.empty()) {
          entry["insertText"] = item.insert_text;
          entry["insertTextFormat"] = item.snippet ? 2 : 1;
        }
        items.push_back(std::move(entry));
      }
      if (info.link_path_completion) {
        link_path_items(doc, info.link_path_prefix, items);
      }
      reply(id, json{{"isIncomplete", false}, {"items", std::move(items)}});
      return;
    }
    if (method == "textDocument/hover") {
      if (!info.has_hover) {
        reply(id, nullptr);
        return;
      }
      const std::string value = info.hover_markdown.empty() ? "```clpp\n" + info.hover.text + "\n```" : info.hover_markdown;
      reply(id, json{{"contents", {{"kind", "markdown"}, {"value", value}}}});
      return;
    }
    if (method == "textDocument/definition") {
      if (!info.has_definition || info.definition.line == 0) {
        reply(id, nullptr);
        return;
      }
      if (info.definition_path.empty()) {
        reply(id, json{{"uri", doc.uri},
                       {"range", range(text, info.definition.line, info.definition.column, info.definition_length)}});
        return;
      }
      if (info.definition_path.front() == '@') {
        reply(id, nullptr);  // built-in module: no file to open
        return;
      }
      const std::filesystem::path target =
          (uri_to_path(doc.uri).parent_path() / std::filesystem::path(info.definition_path)).lexically_normal();
      const std::optional<std::string> module_text = loader_for(doc.uri)(info.definition_path);
      const std::string target_text = module_text.value_or(std::string{});
      reply(id, json{{"uri", path_to_uri(target)},
                     {"range", range(target_text, info.definition.line, info.definition.column, info.definition_length)}});
      return;
    }
    if (method == "textDocument/signatureHelp") {
      if (!info.has_signature) {
        reply(id, nullptr);
        return;
      }
      json parameters = json::array();
      for (const std::string& parameter : info.signature.parameters) {
        parameters.push_back(json{{"label", parameter}});
      }
      reply(id, json{{"signatures", json::array({json{{"label", info.signature.label}, {"parameters", parameters}}})},
                     {"activeSignature", 0},
                     {"activeParameter", info.signature.active}});
      return;
    }
    reply(id, nullptr);
  }

  // Files and folders for `link "./…"`.
  void link_path_items(const Document& doc, const std::string& typed, json& items) const {
    const std::filesystem::path base = uri_to_path(doc.uri).parent_path();
    std::string folder_part = typed;
    const std::size_t slash = folder_part.find_last_of('/');
    folder_part = slash == std::string::npos ? std::string{} : folder_part.substr(0, slash + 1);
    const std::filesystem::path folder = (base / folder_part).lexically_normal();
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
      const std::string name = entry.path().filename().string();
      if (name.empty() || name.front() == '.') {
        continue;
      }
      const bool is_dir = entry.is_directory(error);
      if (!is_dir && entry.path().extension() != ".clp") {
        continue;
      }
      if (!is_dir && key_of(entry.path()) == key_of(uri_to_path(doc.uri))) {
        continue;  // a file cannot link itself
      }
      std::string label = (folder_part.empty() ? "./" : folder_part) + name + (is_dir ? "/" : "");
      if (folder_part.empty() && typed.rfind("../", 0) == 0) {
        continue;
      }
      json entry_json{{"label", label}, {"kind", is_dir ? 19 : 17}, {"detail", is_dir ? "pasta" : "módulo CL++"},
                      {"sortText", std::string(is_dir ? "1" : "0") + label}};
      if (is_dir) {
        entry_json["command"] = json{{"title", "suggest"}, {"command", "editor.action.triggerSuggest"}};
      }
      items.push_back(std::move(entry_json));
    }
    if (typed.empty() || std::string("./").rfind(typed, 0) == 0 || typed == "../") {
      items.push_back(json{{"label", "../"}, {"kind", 19}, {"detail", "pasta acima"}, {"sortText", "2../"}});
    }
  }

  [[nodiscard]] static json document_symbols(const AnalysisResult& analysis, const std::string& text) {
    json symbols = json::array();
    const parser::Program& program = analysis.program;
    const auto imported_type = [&](const std::string& name) {
      for (const ModuleInfo& module : analysis.modules) {
        if (std::find(module.types.begin(), module.types.end(), name) != module.types.end()) {
          return true;
        }
      }
      return false;
    };
    const auto symbol = [&](const std::string& name, const std::string& detail, const int kind,
                            const SourceLocation location) {
      const json where = range(text, std::max<std::uint32_t>(location.line, 1), std::max<std::uint32_t>(location.column, 1),
                               static_cast<std::uint32_t>(name.size()));
      return json{{"name", name}, {"detail", detail}, {"kind", kind}, {"range", where}, {"selectionRange", where}};
    };
    for (const parser::StructDecl& decl : program.structs) {
      if (imported_type(decl.name) || decl.name_location.line == 0) {
        continue;
      }
      json entry = symbol(decl.name, decl.base.empty() ? "struct" : "struct : " + decl.base, 23, decl.name_location);
      json children = json::array();
      for (const parser::Field& field : decl.fields) {
        if ((!field.owner.empty() && field.owner != decl.name) || field.name_location.line == 0) {
          continue;
        }
        children.push_back(symbol(field.name, field.type_name, 8, field.name_location));
      }
      const std::string prefix = decl.name + ".";
      for (const parser::Function& function : program.functions) {
        if (function.name.compare(0, prefix.size(), prefix) == 0 && function.name_location.line != 0) {
          children.push_back(symbol(function.name.substr(prefix.size()), "method", 6, function.name_location));
        }
      }
      entry["children"] = children;
      symbols.push_back(std::move(entry));
    }
    for (const parser::EnumDecl& decl : program.enums) {
      if (imported_type(decl.name) || decl.name_location.line == 0) {
        continue;
      }
      json entry = symbol(decl.name, "enum", 10, decl.name_location);
      json children = json::array();
      for (std::size_t index = 0; index < decl.variants.size() && index < decl.variant_locations.size(); ++index) {
        children.push_back(symbol(decl.variants[index], std::to_string(index), 22, decl.variant_locations[index]));
      }
      entry["children"] = children;
      symbols.push_back(std::move(entry));
    }
    for (const parser::Function& function : program.functions) {
      if (function.name.find('.') != std::string::npos || function.name_location.line == 0) {
        continue;
      }
      symbols.push_back(symbol(function.name, "func", 12, function.name_location));
    }
    for (const parser::Stmt& stmt : program.statements) {
      if (stmt.kind == parser::Stmt::Kind::Let && stmt.name_location.line != 0) {
        symbols.push_back(symbol(stmt.name, stmt.declared_type.empty() ? "let" : stmt.declared_type,
                                 stmt.immutable ? 14 : 13, stmt.name_location));
      }
    }
    return symbols;
  }

  std::istream& m_in;
  std::ostream& m_out;
  std::map<std::string, Document> m_documents;
  bool m_initialized{false};
  bool m_shutdown{false};
};

}  // namespace

int run_lsp(std::istream& in, std::ostream& out) {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  std::ios::sync_with_stdio(false);
  Server server(in, out);
  return server.run();
}

}  // namespace clpp::ide
