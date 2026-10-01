#include "clpp/ide.hpp"
#include "clpp/semantic.hpp"
#include "core/compiler/analyze.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

namespace {

[[nodiscard]] std::string read_source_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}

[[nodiscard]] bool has_word(const std::string& text, const std::string& word) {
  std::size_t pos = 0;
  while ((pos = text.find(word, pos)) != std::string::npos) {
    const bool left = pos == 0 || (std::isalnum(static_cast<unsigned char>(text[pos - 1])) == 0 && text[pos - 1] != '_');
    const std::size_t after = pos + word.size();
    const bool right =
        after >= text.size() || (std::isalnum(static_cast<unsigned char>(text[after])) == 0 && text[after] != '_');
    if (left && right) {
      return true;
    }
    pos = after;
  }
  return false;
}

[[nodiscard]] std::string token_text(const std::string& source, const clpp::ide::SemanticToken& token) {
  std::size_t index = 0;
  std::uint32_t line = 1;
  std::uint32_t column = 1;
  while (index < source.size() && (line < token.line || (line == token.line && column < token.column))) {
    if (source[index] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
    ++index;
  }
  if (index + token.length > source.size()) {
    return {};
  }
  return source.substr(index, token.length);
}

[[nodiscard]] const clpp::ide::SemanticToken* find_token(const std::string& source,
                                                        const std::vector<clpp::ide::SemanticToken>& tokens,
                                                        const std::string& text, const std::uint32_t token_type,
                                                        const std::uint32_t required_modifiers,
                                                        const std::uint32_t excluded_modifiers = 0) {
  for (const clpp::ide::SemanticToken& token : tokens) {
    if (token.token_type == token_type && token_text(source, token) == text &&
        (token.modifiers & required_modifiers) == required_modifiers &&
        (token.modifiers & excluded_modifiers) == 0) {
      return &token;
    }
  }
  return nullptr;
}

[[nodiscard]] bool has_label(const clpp::ide::Info& info, const std::string& label) {
  for (const clpp::ide::Item& item : info.completions) {
    if (item.label == label) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("completion offers a local and hides a block local", "[ide]") {
  const std::string source = "if (1) {\n  let hidden = 1;\n}\nlet shown = 2;\npost(sho";
  const clpp::ide::Info info = clpp::ide::inspect(source, 5, 9);
  REQUIRE(has_label(info, "shown"));
  REQUIRE_FALSE(has_label(info, "hidden"));
}

TEST_CASE("completion offers struct fields after a dot", "[ide]") {
  const std::string source = "struct Point { int x; int y; }\nPoint p = Point(1, 2);\npost(p.";
  const clpp::ide::Info info = clpp::ide::inspect(source, 3, 8);
  REQUIRE(has_label(info, "x"));
  REQUIRE(has_label(info, "y"));
  REQUIRE_FALSE(has_label(info, "let"));
}

TEST_CASE("hover and definition name the local type", "[ide]") {
  const std::string source = "int score = 1;\npost(score);\n";
  const clpp::ide::Info info = clpp::ide::inspect(source, 2, 8);
  REQUIRE(info.has_hover);
  REQUIRE(info.hover.text == "score: int = 1");
  REQUIRE(info.has_definition);
  REQUIRE(info.definition.line == 1);
}

TEST_CASE("analysis owns source buffers for linked module spans", "[analysis]") {
  const clpp::AnalysisResult analysis = clpp::analyze_program(
      "link \"./math.clp\" as Math;\n",
      [](const std::string_view path) -> std::optional<std::string> {
        if (path == "./math.clp") {
          return "func twice(a) { return a + a; }\n";
        }
        return std::nullopt;
      });
  REQUIRE(analysis.ok());
  REQUIRE(analysis.source_buffers.size() == 2);
  REQUIRE(analysis.program.functions.size() == 1);
  REQUIRE(analysis.program.functions[0].name == "Math.twice");
  const std::string_view span = analysis.program.functions[0].body[0].expr.left->span;
  REQUIRE(span == "a");
  REQUIRE(analysis.source_buffers[1].find(span) != std::string::npos);
}

TEST_CASE("completion sees a for-in binding and a vector component", "[ide]") {
  const clpp::ide::Info inside = clpp::ide::inspect("for (let i in 3) {\n  post(i);\n}\n", 2, 8);
  REQUIRE(has_label(inside, "i"));
  const clpp::ide::Info outside = clpp::ide::inspect("for (let i in 3) {\n  post(0);\n}\npost(i);\n", 4, 6);
  REQUIRE_FALSE(has_label(outside, "i"));
  const clpp::ide::Info component = clpp::ide::inspect("Vector4 v = Vector4(1, 2, 3, 4);\npost(v.", 2, 8);
  REQUIRE(has_label(component, "w"));
  REQUIRE_FALSE(has_label(component, "let"));
}

TEST_CASE("format source indents a block and keeps a comment", "[ide]") {
  const std::string formatted = clpp::ide::format_source("if(1){post(1);}else{post(0);}\n<< keep\n");
  REQUIRE(formatted == "if (1) {\n  post(1);\n} else {\n  post(0);\n}\n<< keep\n");
  REQUIRE(clpp::ide::format_source(formatted) == formatted);
}

TEST_CASE("completion offers inherited fields and methods", "[ide]") {
  const std::string source =
      "struct Animal {\n"
      "  int age;\n"
      "  func speak() { return self.age; }\n"
      "}\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "}\n"
      "Dog pet = Dog(1, 2);\n"
      "Animal view = pet;\n"
      "post(pet.\n"
      "post(view.\n";
  const clpp::ide::Info dog = clpp::ide::inspect(source, 10, 10);
  REQUIRE(has_label(dog, "age"));
  REQUIRE(has_label(dog, "bones"));
  REQUIRE(has_label(dog, "speak"));
  const clpp::ide::Info base = clpp::ide::inspect(source, 11, 11);
  REQUIRE(has_label(base, "age"));
  REQUIRE(has_label(base, "speak"));
  REQUIRE_FALSE(has_label(base, "bones"));
}

TEST_CASE("completion and hover remember inferred types and values", "[ide]") {
  const std::string source =
      "struct Animal {\n"
      "  int age;\n"
      "  func speak() { return self.age; }\n"
      "}\n"
      "struct Dog : Animal {\n"
      "  int bones;\n"
      "  func speak() { return self.bones; }\n"
      "}\n"
      "let pet = Dog(4, 9);\n"
      "let copy = pet;\n"
      "Animal view = pet;\n"
      "let mut n = 1;\n"
      "n = 2;\n"
      "post(copy.\n"
      "post(view.\n";
  const clpp::ide::Info pet = clpp::ide::inspect(source, 9, 6);
  REQUIRE(pet.hover.text == "pet: Dog = Dog(4, 9)");
  const clpp::ide::Info copy = clpp::ide::inspect(source, 10, 6);
  REQUIRE(copy.hover.text == "copy: Dog = Dog(4, 9)");
  const clpp::ide::Info view = clpp::ide::inspect(source, 11, 9);
  REQUIRE(view.hover.text == "view: Animal = Dog(4, 9)");
  const clpp::ide::Info number = clpp::ide::inspect(source, 13, 1);
  REQUIRE(number.hover.text == "n: int = 2");
  const clpp::ide::Info derived = clpp::ide::inspect(source, 14, 11);
  REQUIRE(has_label(derived, "age"));
  REQUIRE(has_label(derived, "bones"));
  REQUIRE(has_label(derived, "speak"));
  const clpp::ide::Info base = clpp::ide::inspect(source, 15, 11);
  REQUIRE(has_label(base, "age"));
  REQUIRE(has_label(base, "speak"));
  REQUIRE_FALSE(has_label(base, "bones"));
}

TEST_CASE("fuzzy member completion stays quiet inside a comment", "[ide]") {
  const clpp::ide::Info fuzzy = clpp::ide::inspect(
      "struct PlayerData { int health; }\nPlayerData player = PlayerData(1);\nplayer.hl", 3, 10);
  REQUIRE(has_label(fuzzy, "health"));
  const clpp::ide::Info comment = clpp::ide::inspect("let health = 1;\n<< health", 2, 6);
  REQUIRE_FALSE(has_label(comment, "health"));
}

TEST_CASE("incomplete member access keeps the rest of the file", "[ide]") {
  const std::string source =
      "struct PlayerData { string Name; int Health; int Move; }\n"
      "PlayerData player = PlayerData(\"Ada\", 10, 1);\n"
      "player.\n"
      "int x = \"hi\";\n";
  const clpp::ide::Info info = clpp::ide::inspect(source, 3, 8);
  REQUIRE(has_label(info, "Name"));
  REQUIRE(has_label(info, "Health"));
  REQUIRE(has_label(info, "Move"));
  const std::string broken =
      "let score = 1;\nstruct PlayerData { int Health; }\nPlayerData player = PlayerData(1);\nplayer.\n";
  REQUIRE(has_label(clpp::ide::inspect(broken, 4, 8), "Health"));
  REQUIRE(has_label(clpp::ide::inspect(broken, 1, 6), "score"));
  bool expected_name = false;
  bool mismatch = false;
  for (const clpp::Diagnostic& diagnostic : info.diagnostics) {
    expected_name = expected_name || diagnostic.message == "expected name";
    mismatch = mismatch || diagnostic.message == "type mismatch";
  }
  REQUIRE(expected_name);
  REQUIRE(mismatch);

  clpp::ide::Session session;
  const clpp::ide::Info again = session.open(source, 3, 8);
  const clpp::ide::Info cached = session.open(source, 3, 8);
  REQUIRE(has_label(again, "Health"));
  REQUIRE(has_label(cached, "Move"));
}

TEST_CASE("signature help names the active parameter and locals rank first", "[ide]") {
  const clpp::ide::Info help = clpp::ide::inspect("func add(int x, int y) { return x + y; }\npost(add(1,", 2, 12);
  REQUIRE(help.has_signature);
  REQUIRE(help.signature.label == "add(int x, int y)");
  REQUIRE(help.signature.active == 1);
  const clpp::ide::Info ranked = clpp::ide::inspect("let alpha = 1;\na", 2, 2);
  REQUIRE_FALSE(ranked.completions.empty());
  REQUIRE(ranked.completions.front().label == "alpha");
}

TEST_CASE("inspect reports a type mismatch", "[ide]") {
  const clpp::ide::Info info = clpp::ide::inspect("int x = \"hi\";\n", 1, 1);
  REQUIRE_FALSE(info.diagnostics.empty());
  REQUIRE(info.diagnostics[0].message == "type mismatch");
}

TEST_CASE("textmate grammar lists every keyword and the highlight rules", "[ide]") {
  const std::string root = CLPP_SOURCE_DIR;
  const std::string grammar = read_source_file(root + "/tools/vscode/syntaxes/clpp.tmLanguage.json");
  const std::string lexer = read_source_file(root + "/src/core/lexer/lexer.cpp");
  REQUIRE_FALSE(grammar.empty());
  REQUIRE_FALSE(lexer.empty());
  int keywords = 0;
  const std::string marker = "KeywordEntry{\"";
  std::size_t pos = 0;
  while ((pos = lexer.find(marker, pos)) != std::string::npos) {
    pos += marker.size();
    const std::size_t end = lexer.find('"', pos);
    REQUIRE(end != std::string::npos);
    const std::string word = lexer.substr(pos, end - pos);
    INFO(word);
    REQUIRE(has_word(grammar, word));
    ++keywords;
    pos = end;
  }
  REQUIRE(keywords == 77);
  REQUIRE(grammar.find("comment.line.clpp") != std::string::npos);
  REQUIRE(grammar.find("comment.block.clpp") != std::string::npos);
  REQUIRE(grammar.find("comment.block.documentation.clpp") != std::string::npos);
  REQUIRE(grammar.find("constant.character.escape.clpp") != std::string::npos);
  REQUIRE(grammar.find("keyword.operator.clpp") != std::string::npos);
  REQUIRE(has_word(grammar, "=>"));
  const std::string plus_plus = std::string{'\\', '\\', '+', '\\', '\\', '+'};
  REQUIRE(grammar.find(plus_plus) != std::string::npos);
}

TEST_CASE("semantic tokens distinguish bindings", "[ide]") {
  const std::string source =
      "let n = 1;\n"
      "let mut counter = 2;\n"
      "func add(int x) { return x; }\n"
      "struct Point { int x; }\n"
      "enum Color { Red }\n"
      "async func ping() { return 1; }\n"
      "post(n);\n"
      "post(counter);\n";
  const std::vector<clpp::ide::SemanticToken> tokens = clpp::ide::semantic_tokens(source);
  const clpp::ide::SemanticToken* immutable = find_token(source, tokens, "n", clpp::ide::kSemanticVariable,
                                                         clpp::ide::kSemanticDeclaration | clpp::ide::kSemanticReadonly);
  REQUIRE(immutable != nullptr);
  const clpp::ide::SemanticToken* mutable_name =
      find_token(source, tokens, "counter", clpp::ide::kSemanticVariable, clpp::ide::kSemanticDeclaration);
  REQUIRE(mutable_name != nullptr);
  REQUIRE((mutable_name->modifiers & clpp::ide::kSemanticReadonly) == 0);
  REQUIRE(find_token(source, tokens, "add", clpp::ide::kSemanticFunction, clpp::ide::kSemanticDeclaration) != nullptr);
  const clpp::ide::SemanticToken* parameter =
      find_token(source, tokens, "x", clpp::ide::kSemanticParameter, clpp::ide::kSemanticDeclaration);
  REQUIRE(parameter != nullptr);
  REQUIRE(find_token(source, tokens, "Point", clpp::ide::kSemanticStruct, clpp::ide::kSemanticDeclaration) != nullptr);
  REQUIRE(find_token(source, tokens, "x", clpp::ide::kSemanticProperty, clpp::ide::kSemanticDeclaration) != nullptr);
  REQUIRE(find_token(source, tokens, "Color", clpp::ide::kSemanticEnum, clpp::ide::kSemanticDeclaration) != nullptr);
  REQUIRE(find_token(source, tokens, "Red", clpp::ide::kSemanticEnumMember, clpp::ide::kSemanticDeclaration) != nullptr);
  const clpp::ide::SemanticToken* async_func =
      find_token(source, tokens, "ping", clpp::ide::kSemanticFunction,
                 clpp::ide::kSemanticDeclaration | clpp::ide::kSemanticAsync);
  REQUIRE(async_func != nullptr);
  REQUIRE(find_token(source, tokens, "int", clpp::ide::kSemanticType, 0) != nullptr);
  const clpp::ide::SemanticToken* use_n =
      find_token(source, tokens, "n", clpp::ide::kSemanticVariable, clpp::ide::kSemanticReadonly,
                 clpp::ide::kSemanticDeclaration);
  REQUIRE(use_n != nullptr);
  REQUIRE((use_n->modifiers & clpp::ide::kSemanticDeclaration) == 0);
  const std::vector<std::uint32_t> encoded = clpp::ide::encode_semantic_tokens(tokens);
  REQUIRE(encoded.size() == tokens.size() * 5);
}

TEST_CASE("semantic references keep shadowed names apart", "[semantic]") {
  const clpp::AnalysisResult analysis = clpp::analyze_program(
      "let x = 1;\nfunc inner() {\n  let x = 2;\n  post(x);\n}\npost(x);\n", {});
  REQUIRE(analysis.ok());
  const clpp::SemanticSnapshot snapshot = clpp::build_snapshot(analysis);
  const clpp::SemanticSymbol* inner = nullptr;
  for (const clpp::SemanticSymbol& symbol : snapshot.symbols) {
    if (symbol.name == "x" && !symbol.declaration && symbol.scope != 0) {
      inner = &symbol;
    }
  }
  REQUIRE(inner != nullptr);
  const std::vector<clpp::SemanticSymbol> refs =
      clpp::references_at(snapshot, inner->location.line, inner->location.column);
  REQUIRE(refs.size() == 2);
  for (const clpp::SemanticSymbol& symbol : refs) {
    REQUIRE(symbol.scope != 0);
    REQUIRE(symbol.id == inner->id);
  }
}

TEST_CASE("utf-16 columns count a code point before the cursor as one character", "[semantic]") {
  const std::string line = "caf\xC3\xA9_";
  REQUIRE(clpp::byte_column_from_utf16(line, 0, 4) == 5);
  REQUIRE(clpp::lsp_character_at(line, 1, 6) == 4);
}
