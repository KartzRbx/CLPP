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

#include "clpp/detail/utf8_path.hpp"
#include "clpp/ide.hpp"
#include "clpp/semantic.hpp"
#include "clpp/stdlib.hpp"
#include "clpp/core/lexer/lexer.hpp"
#include "core/compiler/analyze.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>
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
constexpr int kInvalidParams = -32602;
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
  return clpp::detail::utf8_path(decoded).lexically_normal();
}

[[nodiscard]] std::string path_utf8(const std::filesystem::path& path) {
  const auto bytes = path.generic_u8string();
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

[[nodiscard]] std::string path_to_uri(const std::filesystem::path& path) {
  std::string text = path_utf8(path.lexically_normal());
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
  std::string key = path_utf8(path.lexically_normal());
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
      const bool valid_id = !message.contains("id") || message["id"].is_null() ||
          message["id"].is_string() || message["id"].is_number_integer();
      const bool valid_params = !message.contains("params") || message["params"].is_null() ||
          message["params"].is_object() || message["params"].is_array();
      if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0" || !valid_id || !valid_params ||
          (message.contains("method") && !message["method"].is_string())) {
        send_error(valid_id && message.contains("id") ? message["id"] : json(), kInvalidRequest, "invalid JSON-RPC request");
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
      } catch (const json::exception&) {
        if (!id.is_null()) send_error(id, kInvalidParams, "invalid parameters for " + method);
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
    constexpr std::size_t kMaximumHeaderBytes = 16 * 1024;
    constexpr std::size_t kMaximumMessageBytes = 64 * 1024 * 1024;
    std::size_t length = 0;
    std::size_t header_bytes = 0;
    bool saw_length = false;
    bool any_header = false;
    std::string line;
    while (m_in) {
      line.clear();
      char character = 0;
      while (m_in.get(character)) {
        if (++header_bytes > kMaximumHeaderBytes) return false;
        if (character == '\n') break;
        line.push_back(character);
      }
      if (!m_in) return false;
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
        if (saw_length) return false;
        saw_length = true;
        std::string_view value(line.data() + colon + 1, line.size() - colon - 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
        if (value.empty()) return false;
        for (const char digit : value) {
          if (digit < '0' || digit > '9') return false;
          const auto part = static_cast<std::size_t>(digit - '0');
          if (length > (kMaximumMessageBytes - part) / 10) return false;
          length = length * 10 + part;
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
      const std::filesystem::path target = (base / clpp::detail::utf8_path(path)).lexically_normal();
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

  // LSP CompletionItemKind.
  [[nodiscard]] static int completion_kind(const Kind kind, const bool snippet) {
    if (snippet && kind == Kind::Keyword) {
      return 15;  // Snippet
    }
    switch (kind) {
      case Kind::Method:
        return 2;
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
      case Kind::Snippet:
        return 15;
      case Kind::EnumMember:
        return 20;
      case Kind::Constant:
        return 21;
      case Kind::Struct:
        return 22;
      case Kind::Type:
        return 25;  // TypeParameter: the "type" icon, distinct from keywords and structs
    }
    return 1;
  }

  // LSP SymbolKind.
  [[nodiscard]] static int symbol_kind(const Kind kind) {
    switch (kind) {
      case Kind::Method:
        return 6;
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
      case Kind::Constant:
        return 14;
      case Kind::EnumMember:
        return 22;
      case Kind::Type:
        return 26;
      case Kind::Keyword:
      case Kind::Snippet:
        return 14;
    }
    return 13;
  }

  // --- diagnostics ---------------------------------------------------------------------------

  bool safe_rename(const Document& doc, const AnalysisResult& original, const SemanticSnapshot& snapshot,
                   std::vector<SemanticSymbol> matches, const std::string& name) {
    if (matches.empty()) return true;
    std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
      return std::pair{left.location.line, left.location.column} < std::pair{right.location.line, right.location.column};
    });
    std::vector<std::size_t> lines{0};
    for (std::size_t index = 0; index < doc.text.size(); ++index)
      if (doc.text[index] == '\n') lines.push_back(index + 1);
    std::string changed = doc.text;
    // Byte positions belong to the original text; apply the last edit first.
    for (auto iter = matches.rbegin(); iter != matches.rend(); ++iter) {
      if (iter->location.line == 0 || iter->location.line > lines.size()) return false;
      const auto offset = lines[iter->location.line - 1] + iter->location.column - 1;
      if (changed.compare(offset, iter->name.size(), iter->name) != 0) return false;
      changed.replace(offset, iter->name.size(), name);
    }
    const auto candidate = analyze_program(changed, loader_for(doc.uri));
    if (candidate.diagnostics.size() > original.diagnostics.size()) return false;
    const auto next = build_snapshot(candidate);
    if (next.symbols.size() != snapshot.symbols.size()) return false;
    using Position = std::pair<std::uint32_t, std::uint32_t>;
    std::map<Position, const SemanticSymbol*> symbols;
    for (const auto& symbol : next.symbols) symbols.emplace(Position{symbol.location.line, symbol.location.column}, &symbol);
    std::map<std::uint32_t, std::vector<std::pair<std::uint32_t, std::int64_t>>> shifts;
    for (const auto& match : matches) {
      auto& row = shifts[match.location.line];
      const auto previous = row.empty() ? 0 : row.back().second;
      row.emplace_back(match.location.column + static_cast<std::uint32_t>(match.name.size()),
                       previous + static_cast<std::int64_t>(name.size()) - static_cast<std::int64_t>(match.name.size()));
    }
    // A rename must preserve every binding, including unrelated uses that the new
    // name could capture. Checking only the renamed references misses that case.
    for (const auto& before : snapshot.symbols) {
      std::int64_t shift = 0;
      if (const auto found = shifts.find(before.location.line); found != shifts.end()) {
        const auto& row = found->second;
        auto iter = std::upper_bound(row.begin(), row.end(), before.location.column,
            [](const auto column, const auto& entry) { return column < entry.first; });
        if (iter != row.begin()) shift = std::prev(iter)->second;
      }
      const auto column = static_cast<std::uint32_t>(static_cast<std::int64_t>(before.location.column) + shift);
      const auto found = symbols.find(Position{before.location.line, column});
      if (found == symbols.end()) return false;
      const auto& after = *found->second;
      const bool renamed = before.id == matches.front().id && before.global == matches.front().global;
      if (after.id != before.id || after.global != before.global || after.declaration != before.declaration ||
          after.name != (renamed ? name : before.name)) return false;
    }
    return true;
  }

  const AnalysisResult& analysis_for(const Document& doc) {
    const auto key = key_of(uri_to_path(doc.uri));
    auto found = m_analysis_cache.find(key);
    if (found == m_analysis_cache.end()) {
      // Retain a few active document graphs, not a second full workspace index.
      if (m_analysis_cache.size() >= 4) {
        const auto oldest = std::min_element(m_analysis_cache.begin(), m_analysis_cache.end(),
            [](const auto& left, const auto& right) { return left.second.used < right.second.used; });
        m_analysis_cache.erase(oldest);
      }
      CachedAnalysis cached;
      cached.analysis = std::make_unique<AnalysisResult>(analyze_program(doc.text, loader_for(doc.uri)));
      found = m_analysis_cache.emplace(key, std::move(cached)).first;
    }
    found->second.used = ++m_analysis_clock;
    return *found->second.analysis;
  }

  void publish(const Document& doc) {
    const AnalysisResult& analysis = analysis_for(doc);
    json diagnostics = json::array();
    // Publishing errors must not construct completions, hover or references.
    // Retain the IDE's one diagnostic per source position policy.
    std::set<std::pair<std::uint32_t, std::uint32_t>> positions;
    for (const Diagnostic& diagnostic : analysis.diagnostics) {
      if (!positions.emplace(diagnostic.location.line, diagnostic.location.column).second) continue;
      const std::uint32_t line = std::max<std::uint32_t>(diagnostic.location.line, 1);
      const std::uint32_t column = std::max<std::uint32_t>(diagnostic.location.column, 1);
      json item{{"range", range(doc.text, line, column, static_cast<std::uint32_t>(diagnostic.span.size()))},
                {"severity", 1},
                {"source", "clpp"},
                {"message", diagnostic.formatted_message()}};
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
      if (params.contains("workspaceFolders") && params["workspaceFolders"].is_array()) {
        for (const auto& folder : params["workspaceFolders"]) {
          const auto uri = folder.value("uri", "");
          if (uri.rfind("file://", 0) == 0) m_workspace_roots.push_back(uri_to_path(uri));
        }
      }
      if (m_workspace_roots.empty() && params.contains("rootUri") && params["rootUri"].is_string()) {
        const auto uri = params["rootUri"].get<std::string>();
        if (uri.rfind("file://", 0) == 0) m_workspace_roots.push_back(uri_to_path(uri));
      }
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
        m_analysis_cache.clear();
        for (const auto& change : params.value("changes", json::array())) {
          if (change.value("type", 2) != 2) m_module_index_dirty = true;
        }
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
      m_analysis_cache.clear();
      publish_all();  // files that link this one may change too
      return;
    }
    if (method == "textDocument/didChange") {
      Document* doc = document(params);
      if (doc == nullptr) {
        return;
      }
      const auto& identifier = params.at("textDocument");
      const int next_version = identifier.value("version", doc->version);
      if (identifier.contains("version") && next_version <= doc->version) return;
      // Apply a batch transactionally: a malformed later range must not leave
      // the live buffer partially edited, with the old analysis still cached.
      std::string edited = doc->text;
      for (const json& change : params.at("contentChanges")) {
        if (!change.contains("range")) {
          edited = change.at("text").get<std::string>();
          continue;
        }
        // Incremental edit (in case a client sends one).
        const auto [start_line, start_column] = to_compiler(edited, change.at("range").at("start"));
        const auto [end_line, end_column] = to_compiler(edited, change.at("range").at("end"));
        const auto offset = [&](const std::uint32_t line, const std::uint32_t column) {
          std::size_t index = 0;
          std::uint32_t current = 1;
          while (index < edited.size() && current < line) {
            if (edited[index++] == '\n') {
              ++current;
            }
          }
          return std::min(edited.size(), index + column - 1);
        };
        const std::size_t from = offset(start_line, start_column);
        const std::size_t to = offset(end_line, end_column);
        if (from > to) return;
        edited.replace(from, to - from, change.at("text").get<std::string>());
      }
      doc->text = std::move(edited);
      doc->version = next_version;
      m_analysis_cache.clear();
      publish_all();
      return;
    }
    if (method == "textDocument/didSave") {
      m_module_index_dirty = true;
      m_analysis_cache.clear();
      publish_all();
      return;
    }
    if (method == "textDocument/didClose") {
      const std::string uri = params.at("textDocument").at("uri").get<std::string>();
      m_documents.erase(key_of(uri_to_path(uri)));
      m_analysis_cache.clear();
      notify("textDocument/publishDiagnostics", json{{"uri", uri}, {"diagnostics", json::array()}});
      publish_all();  // Importers now see saved disk content instead of the closed buffer.
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
        method == "textDocument/codeAction" || method == "textDocument/documentHighlight" ||
        method == "textDocument/documentColor" || method == "textDocument/colorPresentation") {
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
    json token_types = json::array();
    for (const std::string_view type : semantic_token_types()) {
      token_types.push_back(std::string(type));
    }
    json token_modifiers = json::array();
    for (const std::string_view modifier : semantic_token_modifiers()) {
      token_modifiers.push_back(std::string(modifier));
    }
    return json{
        {"capabilities",
         {{"positionEncoding", "utf-16"},
          {"textDocumentSync", {{"openClose", true}, {"change", 1}, {"save", {{"includeText", false}}}}},
          {"completionProvider",
           {{"triggerCharacters", json::array({".", "@", "\"", "/", ":", "{", ","})}, {"resolveProvider", false}}},
          {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}, {"retriggerCharacters", json::array({","})}}},
          {"hoverProvider", true},
          {"definitionProvider", true},
          {"referencesProvider", true},
          {"documentHighlightProvider", true},
          {"colorProvider", true},
          {"renameProvider", {{"prepareProvider", true}}},
          {"foldingRangeProvider", true},
          {"codeActionProvider", {{"codeActionKinds", json::array({"quickfix"})}}},
          {"documentFormattingProvider", true},
          {"documentSymbolProvider", true},
          {"semanticTokensProvider",
           {{"legend", {{"tokenTypes", token_types}, {"tokenModifiers", token_modifiers}}}, {"full", true}}}}},
        {"serverInfo", {{"name", "clpp-vm"}, {"version", CLPP_VERSION}}}};
  }

  void handle_document_request(const std::string& method, const json& id, const json& params, Document& doc) {
    const std::string& text = doc.text;
    // Color previews are lexical and remain available while the program is incomplete.
    if (method == "textDocument/documentColor") {
      json colors = json::array();
      Lexer lexer(text);
      for (const auto& token : lexer.tokenize().tokens) {
        if (token.type != TokenType::IntLiteral || token.lexeme.size() < 3 ||
            token.lexeme[0] != '0' || (token.lexeme[1] != 'x' && token.lexeme[1] != 'X')) continue;
        std::string digits;
        for (const char c : token.lexeme.substr(2)) if (c != '_') digits.push_back(c);
        if (digits.size() != 6 && digits.size() != 8) continue;
        std::uint32_t packed = 0;
        for (const char c : digits) packed = (packed << 4) | static_cast<std::uint32_t>(hex_value(c));
        colors.push_back(json{{"range", range(text, token.location.line, token.location.column,
                                                static_cast<std::uint32_t>(token.lexeme.size()))},
          {"color", {{"red", ((packed >> 16) & 255) / 255.0}, {"green", ((packed >> 8) & 255) / 255.0},
                     {"blue", (packed & 255) / 255.0}, {"alpha", (255 - ((packed >> 24) & 255)) / 255.0}}}});
      }
      reply(id, colors); return;
    }
    if (method == "textDocument/colorPresentation") {
      const auto& color = params.at("color");
      const auto byte = [&](const char* component) {
        const double value = color.at(component).get<double>();
        return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255));
      };
      const std::uint32_t transparency = 255 - byte("alpha");
      const std::uint32_t packed = (transparency << 24) | (byte("red") << 16) | (byte("green") << 8) | byte("blue");
      std::ostringstream literal;
      literal << "0x" << std::uppercase << std::hex << std::setfill('0')
              << std::setw(transparency == 0 ? 6 : 8) << packed;
      reply(id, json::array({json{{"label", literal.str()},
          {"textEdit", {{"range", params.at("range")}, {"newText", literal.str()}}}}}));
      return;
    }
    if (method == "textDocument/semanticTokens/full") {
      const std::vector<std::uint32_t> data = encode_semantic_tokens(semantic_tokens(analysis_for(doc)), text);
      reply(id, json{{"data", data}});
      return;
    }
    if (method == "textDocument/formatting") {
      const auto options = params.value("options", json::object());
      const auto tab_size = options.value("tabSize", 2u);
      const bool insert_spaces = options.value("insertSpaces", true);
      const std::string formatted = format_source(text, tab_size, insert_spaces);
      int end_line = 0;
      std::uint32_t end_column = 1;
      for (const char c : text) {
        end_line += c == '\n' ? 1 : 0;
        end_column = c == '\n' ? 1 : end_column + 1;
      }
      reply(id, json::array({json{{"range", {{"start", {{"line", 0}, {"character", 0}}},
                                                {"end", position(text, static_cast<std::uint32_t>(end_line + 1), end_column)}}},
                                  {"newText", formatted}}}));
      return;
    }
    const AnalysisResult& analysis = analysis_for(doc);
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
        // Resolve the declaration, so a shadowed name cannot change a different binding.
        const auto [line, column] = to_compiler(text, diagnostic.at("range").at("start"));
        const Info declaration = build_info_for(analysis, text, line, column);
        if (!declaration.has_definition || !declaration.definition_path.empty()) continue;
        for (std::size_t index = 0; index < analysis.tokens.size(); ++index) {
          const Token& target = analysis.tokens[index];
          if (target.location.line != declaration.definition.line ||
              target.location.column != declaration.definition.column) continue;
          for (std::size_t prefix = index; prefix > 0;) {
            const Token& token = analysis.tokens[--prefix];
            if (token.type == TokenType::Semicolon || token.type == TokenType::OpenBrace ||
                token.type == TokenType::CloseBrace) break;
            if (token.type != TokenType::KwConst) continue;
            json edit{{"range", range(text, token.location.line, token.location.column, 5)}, {"newText", ""}};
            actions.push_back(json{{"title", "Make '" + std::string(target.lexeme) + "' mutable (remove const)"},
                                   {"kind", "quickfix"}, {"diagnostics", json::array({diagnostic})},
                                   {"edit", {{"changes", {{doc.uri, json::array({edit})}}}}}});
            break;
          }
          break;
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
      std::string new_name;
      if (method == "textDocument/rename") {
        new_name = params.at("newName").get<std::string>();
        const auto lexical = Lexer(new_name).tokenize();
        if (!lexical.diagnostics.empty() || lexical.tokens.size() != 2 ||
            lexical.tokens.front().type != TokenType::Identifier || lexical.tokens.front().lexeme != new_name) {
          send_error(id, kInvalidParams, "newName must be a single non-reserved CL++ identifier");
          return;
        }
        if (!safe_rename(doc, analysis, snapshot, matches, new_name)) {
          send_error(id, kInvalidParams, "rename would conflict with another declaration or change a binding");
          return;
        }
      }
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
        if (method == "textDocument/references" && symbol.declaration &&
            !params.value("context", json::object()).value("includeDeclaration", false)) continue;
        const json where = range(text, symbol.location.line, symbol.location.column,
                                 static_cast<std::uint32_t>(symbol.name.size()));
        locations.push_back(json{{"uri", doc.uri}, {"range", where}});
        highlights.push_back(json{{"range", where}, {"kind", symbol.declaration ? 3 : 2}});
        if (method == "textDocument/rename") {
          edits.push_back(json{{"range", where}, {"newText", new_name}});
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
      const bool incomplete = !info.link_path_completion && auto_import_items(doc, analysis, line, column, items);
      reply(id, json{{"isIncomplete", incomplete}, {"items", std::move(items)}});
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
          (uri_to_path(doc.uri).parent_path() / clpp::detail::utf8_path(info.definition_path)).lexically_normal();
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

  void refresh_module_index(const std::filesystem::path& base) {
    if (m_workspace_roots.empty() && m_fallback_root != base) {
      m_fallback_root = base;
      m_module_index_dirty = true;
    }
    if (!m_module_index_dirty) return;
    m_module_index_dirty = false;
    m_module_files.clear();
    const auto roots = m_workspace_roots.empty() ? std::vector<std::filesystem::path>{base} : m_workspace_roots;
    std::set<std::string> seen;
    std::size_t visited = 0;
    for (const auto& root : roots) {
      std::error_code error;
      std::filesystem::recursive_directory_iterator iterator(root, std::filesystem::directory_options::skip_permission_denied, error), end;
      for (; iterator != end && visited < 20000 && m_module_files.size() < 5000; iterator.increment(error)) {
        if (error) { error.clear(); continue; }
        ++visited;
        const auto name = path_utf8(iterator->path().filename());
        if (iterator->is_directory(error)) {
          if (iterator.depth() >= 32 || (!name.empty() && name.front() == '.') || name == "node_modules" ||
              name == "build" || name == "dist" || name == "target") iterator.disable_recursion_pending();
          continue;
        }
        if (iterator->path().extension() == ".clp" && iterator->is_regular_file(error) && seen.insert(key_of(iterator->path())).second)
          m_module_files.push_back(iterator->path());
      }
    }
    std::sort(m_module_files.begin(), m_module_files.end());
  }

  // Additional edits travel over standard LSP, so acceptance works in any client.
  bool auto_import_items(const Document& doc, const AnalysisResult& analysis, const std::uint32_t line,
                         const std::uint32_t column, json& items) {
    const auto lexed = Lexer(doc.text).tokenize();
    const Token* word = nullptr;
    std::size_t word_index = 0;
    for (std::size_t index = 0; index < lexed.tokens.size(); ++index) {
      const auto& token = lexed.tokens[index];
      if (token.type == TokenType::Identifier && token.location.line == line && column > token.location.column &&
          column <= token.location.column + token.lexeme.size()) { word = &token; word_index = index; break; }
    }
    if (!word) return false;  // Also excludes strings, comments and empty/member triggers.
    if (word_index > 0 && (lexed.tokens[word_index - 1].type == TokenType::Dot ||
        lexed.tokens[word_index - 1].type == TokenType::Scope || lexed.tokens[word_index - 1].type == TokenType::At)) return false;
    for (std::size_t index = word_index; index > 0;) {
      const auto type = lexed.tokens[--index].type;
      if (type == TokenType::Semicolon || type == TokenType::OpenBrace || type == TokenType::CloseBrace) break;
      if (type == TokenType::KwLink) return false;
    }
    const std::string prefix(word->lexeme.substr(0, column - word->location.column));
    const auto base = uri_to_path(doc.uri).parent_path();
    std::set<std::string> occupied, imported;
    for (const auto& item : items) occupied.insert(item.at("label").get<std::string>());
    for (const auto& link : analysis.links) {
      occupied.insert(link.alias);
      imported.insert(!link.path.empty() && link.path.front() == '@' ? link.path : key_of((base / clpp::detail::utf8_path(link.path)).lexically_normal()));
    }
    std::size_t insertion = 0;
    if (!lexed.tokens.empty() && lexed.tokens.front().type != TokenType::Eof) {
      const auto first = static_cast<std::size_t>(lexed.tokens.front().lexeme.data() - doc.text.data());
      const auto newline = first == 0 ? std::string::npos : doc.text.rfind('\n', first - 1);
      insertion = newline == std::string::npos ? 0 : newline + 1;
    }
    int depth = 0;
    bool header = false;
    for (std::size_t index = 0; index < lexed.tokens.size(); ++index) {
      const auto& token = lexed.tokens[index];
      if (token.type == TokenType::OpenBrace) ++depth;
      if (token.type == TokenType::CloseBrace) --depth;
      if (depth == 0 && (token.type == TokenType::KwLink || token.type == TokenType::KwPragma)) header = true;
      if (header && depth == 0 && token.type == TokenType::Semicolon) {
        insertion = static_cast<std::size_t>(token.lexeme.data() - doc.text.data()) + 1;
        if (index + 1 == lexed.tokens.size() || lexed.tokens[index + 1].type == TokenType::Eof ||
            lexed.tokens[index + 1].location.line > token.location.line) {
          const auto newline = doc.text.find('\n', insertion);
          if (newline != std::string::npos) insertion = newline + 1;
        }
        header = false;
      }
    }
    std::uint32_t insert_line = 1, insert_column = 1;
    for (std::size_t offset = 0; offset < insertion; ++offset) {
      if (doc.text[offset] == '\n') { ++insert_line; insert_column = 1; } else ++insert_column;
    }
    const json insertion_position = position(doc.text, insert_line, insert_column);
    const json insertion_range{{"start", insertion_position}, {"end", insertion_position}};
    const json replacement = range(doc.text, word->location.line, word->location.column, static_cast<std::uint32_t>(word->lexeme.size()));
    const auto word_start = static_cast<std::size_t>(word->lexeme.data() - doc.text.data());
    const std::string eol = doc.text.find("\r\n") == std::string::npos ? "\n" : "\r\n";
    std::size_t count = 0;
    std::set<std::string> suggested;
    const auto offer = [&](const std::string& alias, const std::string& path, const std::string& identity, const bool native) {
      if (alias.size() < prefix.size() || occupied.contains(alias) || imported.contains(identity) || !suggested.insert(identity).second) return;
      for (std::size_t index = 0; index < prefix.size(); ++index)
        if (std::tolower(static_cast<unsigned char>(alias[index])) != std::tolower(static_cast<unsigned char>(prefix[index]))) return;
      if (count >= 200) { ++count; return; }
      const std::string statement = "link " + (native ? path : json(path).dump()) + " as " + alias + ";";
      const std::string import_text = (insertion > 0 && doc.text[insertion - 1] != '\n' ? eol : "") + statement + eol;
      json item{{"label", alias}, {"kind", 9}, {"detail", "Auto-import: " + statement}, {"sortText", "8" + alias + path},
                {"textEdit", {{"range", replacement}, {"newText", alias}}},
                {"documentation", {{"kind", "markdown"}, {"value", "Insert module link:\n```clp\n" + statement + "\n```"}}}};
      if (insertion >= word_start && insertion <= word_start + word->lexeme.size()) {
        item["textEdit"]["newText"] = import_text + eol + alias;  // No overlapping LSP edits at the first word.
      } else item["additionalTextEdits"] = json::array({json{{"range", insertion_range}, {"newText", import_text}}});
      items.push_back(std::move(item));
      ++count;
    };
    for (const auto& module : stdlib::module_catalog()) offer(std::string(module.alias), std::string(module.path), std::string(module.path), true);
    if (doc.uri.rfind("file://", 0) == 0) {
      refresh_module_index(base);
      auto files = m_module_files;
      for (const auto& [key, open] : m_documents) {
        (void)key;
        if (open.uri.rfind("file://", 0) == 0 && uri_to_path(open.uri).extension() == ".clp") files.push_back(uri_to_path(open.uri));
      }
      for (const auto& file : files) {
        if (key_of(file) == key_of(uri_to_path(doc.uri))) continue;
        std::string alias = path_utf8(file.stem());
        for (char& byte : alias) if (static_cast<unsigned char>(byte) < 128 && std::isalnum(static_cast<unsigned char>(byte)) == 0 && byte != '_') byte = '_';
        if (!alias.empty() && std::isdigit(static_cast<unsigned char>(alias.front())) != 0) alias.insert(alias.begin(), '_');
        auto checked = Lexer(alias).tokenize();
        if (!checked.diagnostics.empty() || checked.tokens.empty() || checked.tokens.front().type != TokenType::Identifier) {
          for (char& byte : alias) if (static_cast<unsigned char>(byte) >= 128) byte = '_';
          alias += "Module";
        }
        const auto relative = file.lexically_normal().lexically_relative(base);
        if (relative.empty()) continue;
        auto path = path_utf8(relative);
        if (path.front() != '.') path = "./" + path;
        offer(alias, path, key_of(file), false);
      }
    }
    return count > 200;
  }

  // Files and folders for `link "./…"`.
  void link_path_items(const Document& doc, const std::string& typed, json& items) const {
    const std::filesystem::path base = uri_to_path(doc.uri).parent_path();
    std::string folder_part = typed;
    const std::size_t slash = folder_part.find_last_of('/');
    folder_part = slash == std::string::npos ? std::string{} : folder_part.substr(0, slash + 1);
    const std::filesystem::path folder = (base / clpp::detail::utf8_path(folder_part)).lexically_normal();
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
      const std::string name = path_utf8(entry.path().filename());
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
      json entry_json{{"label", label}, {"kind", is_dir ? 19 : 17}, {"detail", is_dir ? "folder" : "CL++ module"},
                      {"sortText", std::string(is_dir ? "1" : "0") + label}};
      if (is_dir) {
        entry_json["command"] = json{{"title", "suggest"}, {"command", "editor.action.triggerSuggest"}};
      }
      items.push_back(std::move(entry_json));
    }
    if (typed.empty() || std::string("./").rfind(typed, 0) == 0 || typed == "../") {
      items.push_back(json{{"label", "../"}, {"kind", 19}, {"detail", "parent folder"}, {"sortText", "2../"}});
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
      if (decl.imported || imported_type(decl.name) || decl.name_location.line == 0) {
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
      if (decl.imported || imported_type(decl.name) || decl.name_location.line == 0) {
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
      if (stmt.kind == parser::Stmt::Kind::VariableDecl && stmt.name_location.line != 0) {
        symbols.push_back(symbol(stmt.name, stmt.declared_type.empty() ? "auto" : stmt.declared_type,
                                 stmt.immutable ? 14 : 13, stmt.name_location));
      }
    }
    return symbols;
  }

  std::istream& m_in;
  std::ostream& m_out;
  std::map<std::string, Document> m_documents;
  struct CachedAnalysis { std::unique_ptr<AnalysisResult> analysis; std::uint64_t used{0}; };
  std::map<std::string, CachedAnalysis> m_analysis_cache;
  std::uint64_t m_analysis_clock{0};
  std::vector<std::filesystem::path> m_workspace_roots;
  std::vector<std::filesystem::path> m_module_files;
  std::filesystem::path m_fallback_root;
  bool m_module_index_dirty{true};
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
