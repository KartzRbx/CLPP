#pragma once

#include "clpp/compiler.hpp"
#include "clpp/core/lexer/token.hpp"

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {
struct AnalysisResult;
}

namespace clpp::ide {

// What a completion item, symbol or hover is. The LSP layer maps each kind to the matching
// CompletionItemKind / SymbolKind, so the editor shows the right icon ("int" is a type, "post" a
// function, "true" a constant, "match" a keyword).
enum class Kind { Keyword, Function, Variable, Field, Struct, Enum, Module, Type, Constant, Method, EnumMember, Snippet };

struct Item {
  std::string label;
  std::string detail;
  Kind kind{Kind::Keyword};
  std::string sort_text{"5"};
  std::string insert_text;    // empty: insert the label
  bool snippet{false};        // insert_text uses LSP snippet syntax ($1, ${1:name}, $0)
  std::string documentation;  // markdown
};

struct Signature {
  std::string label;
  std::vector<std::string> parameters;
  std::uint32_t active{0};
};

struct Hover {
  std::string text;
  SourceLocation location{};
};

struct SymbolInfo {
  std::string name;
  std::string detail;
  Kind kind{Kind::Variable};
  SourceLocation location{};
};

struct Info {
  std::vector<Diagnostic> diagnostics;
  std::vector<Item> completions;
  std::vector<SymbolInfo> symbols;
  Hover hover;
  bool has_hover{false};
  Signature signature;
  bool has_signature{false};
  SourceLocation definition{};
  bool has_definition{false};
  std::string definition_path;            // empty: the analyzed file; else the `link` path ("./util.clp")
  std::uint32_t definition_length{1};
  std::string hover_markdown;             // rich hover (declaration in a code block + notes)
  bool link_path_completion{false};       // cursor is inside `link "…"`: the host lists files
  std::string link_path_prefix;
};

// Semantic token legend. Indices are part of the wire format: new entries go at the end, and
// semantic_token_types()/semantic_token_modifiers() (used by the LSP and the VS Code client) must
// list them in this order.
inline constexpr std::uint32_t kSemanticDeclaration = 1;
inline constexpr std::uint32_t kSemanticReadonly = 2;
inline constexpr std::uint32_t kSemanticAsync = 4;
inline constexpr std::uint32_t kSemanticDefaultLibrary = 8;  // comes from @clpp.* or the language itself
inline constexpr std::uint32_t kSemanticStatic = 16;

inline constexpr std::uint32_t kSemanticType = 0;
inline constexpr std::uint32_t kSemanticStruct = 1;
inline constexpr std::uint32_t kSemanticEnum = 2;
inline constexpr std::uint32_t kSemanticEnumMember = 3;
inline constexpr std::uint32_t kSemanticParameter = 4;
inline constexpr std::uint32_t kSemanticVariable = 5;
inline constexpr std::uint32_t kSemanticProperty = 6;
inline constexpr std::uint32_t kSemanticFunction = 7;
inline constexpr std::uint32_t kSemanticKeyword = 8;
inline constexpr std::uint32_t kSemanticNamespace = 9;   // module alias (`Axiom` in `Axiom.Clamp`)
inline constexpr std::uint32_t kSemanticMethod = 10;     // `hero.damage(5)`, `func damage(...)` inside a struct
inline constexpr std::uint32_t kSemanticTypeParameter = 11;  // `T` in `struct Box<T>`

[[nodiscard]] const std::vector<std::string_view>& semantic_token_types();
[[nodiscard]] const std::vector<std::string_view>& semantic_token_modifiers();

struct SemanticToken {
  std::uint32_t line{1};
  std::uint32_t column{1};
  std::uint32_t length{0};
  std::uint32_t token_type{kSemanticVariable};
  std::uint32_t modifiers{0};
};

[[nodiscard]] std::vector<SemanticToken> semantic_tokens(std::string_view source, const ModuleLoader& modules = {});

// Delta encoding of LSP `textDocument/semanticTokens`. Without `source`, columns and lengths stay in
// bytes (the compiler's unit); with it they are converted to UTF-16 code units, which is what the
// protocol requires (a line with "ação" before a name would otherwise shift every token after it).
[[nodiscard]] std::vector<std::uint32_t> encode_semantic_tokens(const std::vector<SemanticToken>& tokens);
[[nodiscard]] std::vector<std::uint32_t> encode_semantic_tokens(const std::vector<SemanticToken>& tokens,
                                                                std::string_view source);

// Documentation of a reserved word (`const`, `match`, `int`, `post`...): what it is, for the
// completion list and for hover. Null for anything else.
struct KeywordDoc {
  std::string_view name;
  Kind kind;
  std::string_view detail;  // short, shown next to the suggestion
  std::string_view doc;     // markdown
};
[[nodiscard]] const KeywordDoc* keyword_doc(std::string_view word);

[[nodiscard]] std::string format_source(std::string_view source);

[[nodiscard]] Info inspect(std::string_view source, std::uint32_t line, std::uint32_t column,
                              const ModuleLoader& modules = {});

// Completion, hover, definition and signature for one position of an already analyzed file.
[[nodiscard]] Info build_info_for(const AnalysisResult& analysis, std::string_view source, std::uint32_t line,
                                  std::uint32_t column);

class Session {
 public:
  Session();
  ~Session();
  Session(Session&&) noexcept;
  Session& operator=(Session&&) noexcept;
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  [[nodiscard]] Info open(std::string_view source, std::uint32_t line, std::uint32_t column,
                          const ModuleLoader& modules = {});

 private:
  struct State;
  std::unique_ptr<State> state;
};

[[nodiscard]] int run_lsp(std::istream& in, std::ostream& out);

}  // namespace clpp::ide
