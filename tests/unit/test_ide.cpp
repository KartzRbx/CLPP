#include "clpp/ide.hpp"
#include "clpp/semantic.hpp"
#include "core/compiler/analyze.hpp"
#include "clpp/core/lexer/lexer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

TEST_CASE("type first expressions retain alias hover and type semantic roles", "[ide][type-ascription]") {
  const std::string source = "def Outcome = \"ongoing\" | \"team_a\" | \"team_b\";\nauto result = (Outcome \"ongoing\");\npost(result);\n";
  const auto info = clpp::ide::inspect(source, 2, 18);
  REQUIRE(info.has_hover);
  CHECK(info.hover.text.find("ongoing") != std::string::npos);
  const auto tokens = clpp::ide::semantic_tokens(source);
  bool found = false;
  for (const auto& token : tokens) {
    if (token.line == 2 && token.column == 16) {
      found = true;
      CHECK(token.token_type == clpp::ide::kSemanticType);
    }
  }
  CHECK(found);
  const std::string grouped = "def Count = number;\nauto result = (Count (2 + 3));\npost(result);\n";
  const auto formatted = clpp::ide::format_source(grouped);
  CHECK(formatted.find("Count (") != std::string::npos);
  CHECK(clpp::analyze_program(formatted, {}).ok());
}

TEST_CASE("formatter preserves quoted imports strings and nested templates", "[ide][formatter-roundtrip]") {
  const std::string source = R"clp(link "./module.clp" as Module;
array<string> values = array<string>("", 'ação', "a\\b\"c", "<< not a comment", "]>>");
post(Module.Read()); post(values[1]);
post(`plain`); post(`${values[1]}: ${`nested ${1 + 2}`}`);
<< real comment
)clp";
  const auto loader = [](std::string_view path) -> std::optional<std::string> {
    if (path == "./module.clp") return "export string Read() { return \"module\"; }";
    return std::nullopt;
  };
  REQUIRE(clpp::analyze_program(source, loader).ok());
  const auto formatted = clpp::ide::format_source(source);
  CHECK(formatted.find("\"./module.clp\"") != std::string::npos);
  CHECK(formatted.find("'ação'") != std::string::npos);
  CHECK(formatted.find("`${values[1]}: ${`nested ${1 + 2}`}`") != std::string::npos);
  CHECK(clpp::analyze_program(formatted, loader).ok());
  CHECK(clpp::ide::format_source(formatted) == formatted);
  const auto original_tokens = clpp::Lexer(source).tokenize();
  const auto new_tokens = clpp::Lexer(formatted).tokenize();
  REQUIRE(original_tokens.tokens.size() == new_tokens.tokens.size());
  for (std::size_t index = 0; index < original_tokens.tokens.size(); ++index) {
    CHECK(original_tokens.tokens[index].type == new_tokens.tokens[index].type);
    CHECK(original_tokens.tokens[index].lexeme == new_tokens.tokens[index].lexeme);
  }
}

TEST_CASE("formatter leaves malformed literals untouched", "[ide][formatter-roundtrip]") {
  for (const std::string source : {"post(\"unfinished", "post(`unfinished", "link './unfinished", "<<[ unfinished"}) {
    CHECK(clpp::ide::format_source(source) == source);
  }
}

TEST_CASE("authored comments suppress semantic roles and preserve formatter trivia", "[ide][comments]") {
  const std::string source = "<< 5) def com HERANÇA nominal.\n<<[ number Hidden; ]>>\n";
  REQUIRE(clpp::ide::semantic_tokens(source).empty());
  REQUIRE(clpp::ide::inspect(source, 1, 20).completions.empty());
  REQUIRE(clpp::ide::inspect(source, 2, 15).completions.empty());
  const std::string formatted = clpp::ide::format_source(source + "post(1);");
  REQUIRE(formatted.find("<< 5) def com HERANÇA nominal.") != std::string::npos);
  REQUIRE(formatted.find("<<[ number Hidden; ]>>") != std::string::npos);
  REQUIRE(clpp::ide::format_source(formatted) == formatted);
}

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
  const std::string source = "if (1) {\n  const auto hidden = 1;\n}\nconst auto shown = 2;\npost(sho";
  const clpp::ide::Info info = clpp::ide::inspect(source, 5, 9);
  REQUIRE(has_label(info, "shown"));
  REQUIRE_FALSE(has_label(info, "hidden"));
}

TEST_CASE("completion offers struct fields after a dot", "[ide]") {
  const std::string source = "def Point { number x; number y; }\n"
  "Point p = Point { x = 1, y = 2 };\n"
  "post(p.";
  const clpp::ide::Info info = clpp::ide::inspect(source, 3, 8);
  REQUIRE(has_label(info, "x"));
  REQUIRE(has_label(info, "y"));
  REQUIRE_FALSE(has_label(info, "let"));
}

TEST_CASE("hover and definition name the local type", "[ide]") {
  const std::string source = "number score = 1;\npost(score);\n";
  const clpp::ide::Info info = clpp::ide::inspect(source, 2, 8);
  REQUIRE(info.has_hover);
  REQUIRE(info.hover.text == "score: number = 1");
  REQUIRE(info.has_definition);
  REQUIRE(info.definition.line == 1);
}

TEST_CASE("analysis owns source buffers for linked module spans", "[analysis]") {
  const clpp::AnalysisResult analysis = clpp::analyze_program(
      "link \"./math.clp\" as Math;\n",
      [](const std::string_view path) -> std::optional<std::string> {
        if (path == "./math.clp") {
          return "export number twice(number a) { return a + a; }\n";
        }
        return std::nullopt;
      });
  REQUIRE(analysis.ok());
  REQUIRE(analysis.source_buffers.size() == 2);
  REQUIRE(analysis.program.functions.size() == 1);
  REQUIRE(analysis.program.resolved_functions.size() == 1);
  REQUIRE(analysis.program.resolved_functions.at(0).spelling == "Math.twice");
  REQUIRE(analysis.program.functions.at(0).name == analysis.program.resolved_functions.at(0).identity);
  const std::string_view span = analysis.program.functions.at(0).body.at(0).expr.left->span;
  REQUIRE(span == "a");
  REQUIRE(analysis.source_buffers.at(1).find(span) != std::string::npos);
}

TEST_CASE("completion sees a for-in binding and a vector component", "[ide]") {
  const clpp::ide::Info inside = clpp::ide::inspect("for (auto i in 3) {\n  post(i);\n}\n", 2, 8);
  REQUIRE(has_label(inside, "i"));
  const clpp::ide::Info outside = clpp::ide::inspect("for (auto i in 3) {\n  post(0);\n}\npost(i);\n", 4, 6);
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
      "def Animal {\n"
      "  number age;\n"
      "  number speak() { return @this.age; }\n"
      "}\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "}\n"
      "Dog pet = Dog { age = 1, bones = 2 };\n"
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
      "def Animal {\n"
      "  number age;\n"
      "  number speak() { return @this.age; }\n"
      "}\n"
      "def Dog : Animal {\n"
      "  number bones;\n"
      "  number speak() { return @this.bones; }\n"
      "}\n"
      "const auto pet = Dog { age = 4, bones = 9 };\n"
      "const auto copy = pet;\n"
      "Animal view = pet;\n"
      "auto n = 1;\n"
      "n = 2;\n"
      "post(copy.\n"
      "post(view.\n";
  const clpp::ide::Info pet = clpp::ide::inspect(source, 9, 13);
  REQUIRE(pet.hover.text == "pet: Dog = Dog { age = 4, bones = 9 }");
  const clpp::ide::Info copy = clpp::ide::inspect(source, 10, 13);
  REQUIRE(copy.hover.text == "copy: Dog = Dog { age = 4, bones = 9 }");
  const clpp::ide::Info view = clpp::ide::inspect(source, 11, 9);
  REQUIRE(view.hover.text == "view: Animal = Dog { age = 4, bones = 9 }");
  const clpp::ide::Info number = clpp::ide::inspect(source, 13, 1);
  REQUIRE(number.hover.text == "n: number = 2");
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
      "def PlayerData { number health; }\n"
      "PlayerData player = PlayerData { health = 1 };\n"
      "player.hl", 3, 10);
  REQUIRE(has_label(fuzzy, "health"));
  const clpp::ide::Info comment = clpp::ide::inspect("const auto health = 1;\n<< health", 2, 6);
  REQUIRE_FALSE(has_label(comment, "health"));
}

TEST_CASE("incomplete member access keeps the rest of the file", "[ide]") {
  const std::string source =
      "def PlayerData { string Name; number Health; number Move; }\n"
      "PlayerData player = PlayerData { Name = \"Ada\", Health = 10, Move = 1 };\n"
      "player.\n"
      "number x = \"hi\";\n";
  const clpp::ide::Info info = clpp::ide::inspect(source, 3, 8);
  REQUIRE(has_label(info, "Name"));
  REQUIRE(has_label(info, "Health"));
  REQUIRE(has_label(info, "Move"));
  const std::string broken =
      "const auto score = 1;\n"
      "def PlayerData { number Health; }\n"
      "PlayerData player = PlayerData { Health = 1 };\n"
      "player.\n";
  REQUIRE(has_label(clpp::ide::inspect(broken, 4, 8), "Health"));
  REQUIRE(has_label(clpp::ide::inspect(broken, 1, 13), "score"));
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
  const clpp::ide::Info help = clpp::ide::inspect("number add(number x, number y) { return x + y; }\npost(add(1,", 2, 12);
  REQUIRE(help.has_signature);
  REQUIRE(help.signature.label == "number add(number x, number y)");
  REQUIRE(help.signature.active == 1);
  const clpp::ide::Info ranked = clpp::ide::inspect("const auto alpha = 1;\na", 2, 2);
  REQUIRE_FALSE(ranked.completions.empty());
  REQUIRE(ranked.completions.front().label == "alpha");
}

TEST_CASE("inspect reports a type mismatch", "[ide]") {
  const clpp::ide::Info info = clpp::ide::inspect("number x = \"hi\";\n", 1, 1);
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
    if (word == "struct" || word == "variant" || word == "type" || word == "namespace" ||
        word == "import" || word == "from" || word == "using" || word == "new" ||
        word == "cout" || word == "endl" || word == "static") {
      REQUIRE(grammar.find("|" + word + "|") == std::string::npos);
      REQUIRE(grammar.find("(" + word + "|") == std::string::npos);
      REQUIRE(grammar.find("|" + word + ")") == std::string::npos);
      pos = end;
      continue; // Reserved only for an obsolete-syntax diagnostic, not completion/highlight.
    }
    INFO(word);
    REQUIRE(has_word(grammar, word));
    ++keywords;
    pos = end;
  }
  REQUIRE(keywords == 71);
  REQUIRE(grammar.find("comment.line.clpp") != std::string::npos);
  REQUIRE(grammar.find("<<(?!\\\\[)") != std::string::npos);
  REQUIRE(grammar.find("comment.block.clpp") != std::string::npos);
  REQUIRE(grammar.find("constant.character.escape.clpp") != std::string::npos);
  REQUIRE(grammar.find("keyword.operator.clpp") != std::string::npos);
  REQUIRE(has_word(grammar, "=>"));
  const std::string plus_plus = std::string{'\\', '\\', '+', '\\', '\\', '+'};
  REQUIRE(grammar.find(plus_plus) != std::string::npos);
}

TEST_CASE("semantic tokens distinguish bindings", "[ide]") {
  const std::string source =
      "const auto n = 1;\n"
      "auto counter = 2;\n"
      "number add(number x) { return x; }\n"
      "def Point { number x; }\n"
      "enum class Color { Red }\n"
      "async number ping() { return 1; }\n"
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
  REQUIRE(find_token(source, tokens, "number", clpp::ide::kSemanticType, 0) != nullptr);
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
      "const auto x = 1;\nvoid inner() {\n  const auto x = 2;\n  post(x);\n}\npost(x);\n", {});
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

TEST_CASE("formatter retains standard block and line comments", "[ide][comments]") {
  const std::string source = "<<[ before\n   the program ]>>\npost(1); << after\n";
  const std::string formatted = clpp::ide::format_source(source);
  REQUIRE(formatted.find("<<[ before\n   the program ]>>") != std::string::npos);
  REQUIRE(formatted.find("<< after") != std::string::npos);
  REQUIRE(formatted.find("post(1);") != std::string::npos);
  REQUIRE(clpp::ide::format_source(formatted) == formatted);
  const auto info = clpp::ide::inspect("number health = 1;\n<<[ health ]>>", 2, 6);
  REQUIRE_FALSE(has_label(info, "health"));
}



TEST_CASE("editor resolves inferred factory field and element properties", "[ide][properties]") {
  const std::string source =
      "def Stats { number Health; } def Player { Stats Data; }\n"
      "Player create() { return Player { Data = Stats { Health = 100 } }; }\n"
      "array<Player> team() { return array<Player>(create()); }\n"
      "auto player = create(); auto stats = player.Data; auto first = team()[0];\n"
      "post(player.Data.Health); post(stats.Health); post(first.Data.Health);\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const clpp::ide::Info variable = clpp::ide::inspect(source, 5, 8);
  REQUIRE(variable.has_hover);
  REQUIRE(variable.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(variable.has_definition);
  REQUIRE(variable.definition.line == 4);
  REQUIRE(variable.definition.column == 6);
  REQUIRE(has_label(clpp::ide::inspect(source + "pla", 6, 4), "player"));
  const std::string player_source = source + "player.";
  REQUIRE(has_label(clpp::ide::inspect(player_source, 6, 8), "Data"));
  const std::string stats_source = source + "stats.";
  REQUIRE(has_label(clpp::ide::inspect(stats_source, 6, 7), "Health"));
  const std::string first_source = source + "first.";
  REQUIRE(has_label(clpp::ide::inspect(first_source, 6, 7), "Data"));
}

TEST_CASE("editor completes scoped enum cases", "[ide][scoped-enum]") {
  const std::string source = "enum class Direction { Left, Right }\nDirection::";
  REQUIRE(has_label(clpp::ide::inspect(source, 2, 12), "Left"));
  REQUIRE(has_label(clpp::ide::inspect(source, 2, 12), "Right"));
}

TEST_CASE("alias hover shows its def body at declarations and return types", "[ide][alias-hover]") {
  const std::string source = "def Outcome = \"ongoing\" | \"team_a\" | \"team_b\";\n"
      "Outcome RunBattle() { return \"ongoing\"; }\nOutcome result = RunBattle();\n";
  const std::string expected = "def Outcome = \"ongoing\" | \"team_a\" | \"team_b\";";
  for (const auto [line, column] : {std::pair{1, 6}, std::pair{2, 3}, std::pair{3, 3}}) {
    const auto info = clpp::ide::inspect(source, line, column);
    REQUIRE(info.has_hover); CHECK(info.hover.text == expected);
    CHECK(info.hover_markdown.find(expected) != std::string::npos);
  }
  const auto completion = clpp::ide::inspect(source + "Out", 4, 4);
  for (const auto& item : completion.completions) if (item.label == "Outcome") {
    CHECK(item.detail == expected); CHECK(item.documentation.find(expected) != std::string::npos);
  }
  const std::string callback = "def Transform<T> = (T, string)->T;\nTransform<number>";
  CHECK(clpp::ide::inspect(callback, 2, 4).hover_markdown.find("def Transform<T> = (T, string) -> T;") != std::string::npos);
  const std::string literal = "def Labels = \"a|b\" | \"c&d\";\nLabels";
  CHECK(clpp::ide::inspect(literal, 2, 3).hover_markdown.find("def Labels = \"a|b\" | \"c&d\";") != std::string::npos);
}

TEST_CASE("module alias hover expands exported definitions without internal names", "[ide][alias-hover]") {
  const clpp::ModuleLoader loader = [](std::string_view) -> std::optional<std::string> {
    return "export def Outcome = \"ongoing\" | \"team_a\" | \"team_b\";";
  };
  const std::string source = "link \"./battle.clp\" as Battle;\nBattle.Outcome Run() { return \"ongoing\"; }\n";
  const auto info = clpp::ide::inspect(source, 2, 10, loader);
  REQUIRE(info.has_hover);
  CHECK(info.hover_markdown.find("def Battle.Outcome = \"ongoing\" | \"team_a\" | \"team_b\";") != std::string::npos);
  CHECK(info.hover_markdown.find("$m") == std::string::npos);
}

TEST_CASE("module nominal identities keep IDE members and origins separate", "[ide][modules]") {
  const clpp::ModuleLoader loader = [](std::string_view path) -> std::optional<std::string> {
    if (path == "./left.clp") return "export def Player { number LeftOnly = 1; } export Player Make() { return Player {}; } export enum class Mode { Ready }";
    if (path == "./right.clp") return "export def Player { number RightOnly = 2; } export Player Make() { return Player {}; }";
    return std::nullopt;
  };
  const std::string imports = "link \"./left.clp\" as Left;\nlink \"./right.clp\" as Right;\n";
  const auto members = clpp::ide::inspect(imports + "auto p = Left.Make();\np.", 4, 3, loader);
  REQUIRE(has_label(members, "LeftOnly"));
  REQUIRE_FALSE(has_label(members, "RightOnly"));
  for (const auto& item : members.completions) REQUIRE(item.documentation.find("$m") == std::string::npos);
  const auto hover = clpp::ide::inspect(imports + "auto p = Left.Make();\npost(p.LeftOnly);", 4, 10, loader);
  REQUIRE(hover.has_definition);
  REQUIRE(hover.definition_path == "./left.clp");
  REQUIRE(hover.hover_markdown.find("$m") == std::string::npos);
  const auto definition = clpp::ide::inspect(imports + "Left.Player p = Left.Player {};", 3, 8, loader);
  REQUIRE(definition.has_definition);
  REQUIRE(definition.definition_path == "./left.clp");
  const auto cases = clpp::ide::inspect(imports + "Left.Mode::", 3, 12, loader);
  REQUIRE(has_label(cases, "Ready"));
  const auto root = clpp::ide::inspect(imports + "", 3, 1, loader);
  REQUIRE_FALSE(has_label(root, "Player"));
  REQUIRE_FALSE(has_label(root, "Mode"));
  const std::string source = imports + "Left.Mode mode = Left.Mode::Ready;";
  const auto tokens = clpp::ide::semantic_tokens(source, loader);
  REQUIRE(find_token(source, tokens, "Mode", clpp::ide::kSemanticEnum, 0) != nullptr);
  REQUIRE(find_token(source, tokens, "Ready", clpp::ide::kSemanticEnumMember, 0) != nullptr);
}


TEST_CASE("editor signatures use type-first functions and def summaries", "[ide][canonical]") {
  const std::string source = "def Player { number Health; number Read() { return @this.Health; } }\n"
      "number Add(number left, number right) { return left + right; }\n"
      "Player player = Player { Health = 100 };\npost(Add(1, 2));\nplayer.Read();\n";
  const auto function = clpp::ide::inspect(source, 2, 9);
  REQUIRE(function.has_hover);
  REQUIRE(function.hover_markdown.find("number Add(number left, number right)") != std::string::npos);
  const auto help = clpp::ide::inspect(source, 4, 13);
  REQUIRE(help.has_signature);
  REQUIRE(help.signature.label == "number Add(number left, number right)");
  REQUIRE(help.signature.parameters.size() == 2);
  REQUIRE(help.signature.active == 1);
  const auto data = clpp::ide::inspect(source, 3, 3);
  REQUIRE(data.has_hover);
  REQUIRE(data.hover_markdown.find("def Player") != std::string::npos);
  REQUIRE(data.hover_markdown.find("number Read()") != std::string::npos);
}

TEST_CASE("editor preserves inferred generic return types for local properties", "[ide][generic-inference]") {
  const std::string source = "def Player { number Health; }\n"
      "template <typename T> T Identity(T value) { return value; }\n"
      "auto player = Identity(Player { Health = 15 });\n"
      "post(player.Health);\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const auto hover = clpp::ide::inspect(source, 4, 8);
  REQUIRE(hover.has_hover);
  REQUIRE(hover.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(hover.has_definition);
  REQUIRE(hover.definition.line == 3);
  REQUIRE(has_label(clpp::ide::inspect(source + "player.", 5, 8), "Health"));
}

TEST_CASE("editor preserves the concrete result of methods on generic data", "[ide][generic-methods]") {
  const std::string source = "def Player { number Health; }\n"
      "def Box<T> { T Value; T Get() { return @this.Value; } }\n"
      "auto box = Box<Player> { Value = Player { Health = 21 } };\n"
      "auto player = box.Get();\npost(player.Health);\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const auto hover = clpp::ide::inspect(source, 5, 8);
  REQUIRE(hover.has_hover);
  REQUIRE(hover.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(has_label(clpp::ide::inspect(source + "player.", 6, 8), "Health"));
}

TEST_CASE("editor preserves explicit method template return types", "[ide][method-templates]") {
  const std::string source = "def Player { number Health; }\n"
      "def Box<T> { T Value; template <typename U> U Echo(U value) { return value; } }\n"
      "auto box = Box<number> { Value = 1 };\n"
      "auto player = box.Echo<Player>(Player { Health = 28 });\npost(player.Health);\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const auto hover = clpp::ide::inspect(source, 5, 8);
  REQUIRE(hover.has_hover); REQUIRE(hover.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(has_label(clpp::ide::inspect(source + "player.", 6, 8), "Health"));
}

TEST_CASE("editor resolves concrete inherited generic method results", "[ide][generic-inheritance]") {
  const std::string source = "def Player { number Health; }\n"
      "def Base<T> { T Value; T Get() { return @this.Value; } }\n"
      "def Child<U> : Base<U> {}\n"
      "auto child = Child<Player> { Value = Player { Health = 38 } };\n"
      "auto player = child.Get();\npost(player.Health);\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const auto hover = clpp::ide::inspect(source, 6, 8);
  REQUIRE(hover.has_hover);
  REQUIRE(hover.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(has_label(clpp::ide::inspect(source + "player.", 7, 8), "Health"));
}

TEST_CASE("editor resolves inferred factory iteration elements", "[ide][iteration-types]") {
  const std::string source = "def Player { number Health; }\n"
      "array<Player> MakeTeam() { return array<Player>(Player { Health = 42 }); }\n"
      "for (auto player in MakeTeam()) {\npost(player.Health);\n}\n";
  REQUIRE(clpp::analyze_program(source, {}).ok());
  const auto hover = clpp::ide::inspect(source, 4, 8);
  REQUIRE(hover.has_hover);
  REQUIRE(hover.hover.text.find("player: Player") != std::string::npos);
  REQUIRE(hover.has_definition); REQUIRE(hover.definition.line == 3);
  const std::string partial = source.substr(0, source.find("post(player.Health)")) + "player.";
  REQUIRE(has_label(clpp::ide::inspect(partial, 4, 8), "Health"));
}


TEST_CASE("nested captures preserve the originating semantic binding", "[semantic][callbacks]") {
  const auto analysis = clpp::analyze_program(R"(
    number count = 0;
    auto factory = func() -> () -> number {
      return func() -> number { count = 1; return count; };
    };
    post(count);
  )", {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto snapshot = clpp::build_snapshot(analysis);
  const clpp::SemanticSymbol* declaration = nullptr;
  for (const auto& symbol : snapshot.symbols) if (symbol.name == "count" && symbol.declaration) declaration = &symbol;
  REQUIRE(declaration != nullptr);
  const auto refs = clpp::references_at(snapshot, declaration->location.line, declaration->location.column);
  CHECK(refs.size() == 4);
  for (const auto& symbol : refs) CHECK(symbol.id == declaration->id);
}


TEST_CASE("module global references never merge with entry local slots", "[modules-state][semantic]") {
  const auto analysis = clpp::analyze_program("link \"./m.clp\" as M;\nnumber own = 2;\npost(M.LIMIT);\npost(own);\npost(M.LIMIT);",
    [](std::string_view) -> std::optional<std::string> { return "export const number LIMIT = 7;"; });
  REQUIRE(analysis.ok()); const auto snapshot = clpp::build_snapshot(analysis);
  const clpp::SemanticSymbol* global = nullptr;
  for (const auto& symbol : snapshot.symbols) if (symbol.global && symbol.name == "LIMIT") { global = &symbol; break; }
  REQUIRE(global != nullptr); CHECK(global->name == "LIMIT");
  const auto refs = clpp::references_at(snapshot, global->location.line, global->location.column);
  CHECK(refs.size() == 2); for (const auto& symbol : refs) CHECK(symbol.global);
}


TEST_CASE("entry global references span functions and keep local shadows separate", "[entry-scope][semantic]") {
  const auto analysis = clpp::analyze_program("number value = 1;\nnumber Read() { return value; }\nvoid Change() { value = 2; }\nnumber Local(number value) { return value; }\npost(value);", {});
  REQUIRE(analysis.ok()); const auto snapshot = clpp::build_snapshot(analysis);
  const auto refs = clpp::references_at(snapshot, 1, 8);
  REQUIRE(refs.size() == 4);
  for (const auto& symbol : refs) { CHECK(symbol.global); CHECK(symbol.name == "value"); }
  const auto local = clpp::references_at(snapshot, 4, 21);
  REQUIRE(local.size() == 2);
  for (const auto& symbol : local) CHECK_FALSE(symbol.global);
}


TEST_CASE("editor preserves concrete generic callback signatures and returned properties", "[generic-callback][ide]") {
  const std::string source = "template <typename T> T Identity(T value) { return value; }\n"
    "def Player { number Health; }\n"
    "auto callback = Identity<Player>;\n"
    "auto player = callback(Player { Health = 42 });\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto hover = clpp::ide::inspect(source, 3, 8);
  REQUIRE(hover.has_hover);
  CHECK(hover.hover.text.find("(Player)->Player") != std::string::npos);
  CHECK(has_label(clpp::ide::inspect(source + "player.", 6, 8), "Health"));
  const auto player = clpp::ide::inspect(source, 4, 8);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("player: Player") != std::string::npos);
}

TEST_CASE("editor preserves results inferred through composed generic aliases", "[generic-composition][ide]") {
  const std::string source = "def Choice<T> = T|string;\n"
    "template <typename T> T Keep(Choice<T> value, T fallback) { return fallback; }\n"
    "def Player { number Health; }\n"
    "auto player = Keep(\"ignored\", Player { Health = 42 });\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  REQUIRE(has_label(clpp::ide::inspect(source + "player.", 6, 8), "Health"));
  const auto hover = clpp::ide::inspect(source, 4, 8);
  REQUIRE(hover.has_hover);
  CHECK(hover.hover.text.find("Player") != std::string::npos);
}


TEST_CASE("editor and references preserve generic closure results and captures", "[generic-capture][ide][semantic]") {
  const std::string source = "def Getter<T> = () -> T;\n"
    "template <typename T> Getter<T> Keep(T value) {\n"
    "return func() -> T { return value; };\n"
    "}\n"
    "def Player { number Health; }\n"
    "auto kept = Keep(Player { Health = 42 });\n"
    "auto player = kept();\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto hover = clpp::ide::inspect(source, 6, 8);
  REQUIRE(hover.has_hover); CHECK(hover.hover.text.find("()->Player") != std::string::npos);
  const auto player = clpp::ide::inspect(source, 7, 8);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("player: Player") != std::string::npos);
  CHECK(has_label(clpp::ide::inspect(source + "player.", 9, 8), "Health"));
  const auto snapshot = clpp::build_snapshot(analysis);
  const clpp::SemanticSymbol* parameter = nullptr;
  for (const auto& symbol : snapshot.symbols) if (symbol.name == "value" && symbol.declaration) parameter = &symbol;
  REQUIRE(parameter != nullptr);
  const auto refs = clpp::references_at(snapshot, parameter->location.line, parameter->location.column);
  REQUIRE(refs.size() == 2);
  for (const auto& symbol : refs) CHECK(symbol.id == parameter->id);
}

TEST_CASE("editor resolves contextual lambda parameters and their references", "[contextual-callback][ide][semantic]") {
  const std::string source = "def Player { number Health; }\n"
    "def Reader = (Player) -> number;\n"
    "Reader read = func(player) -> number { return player.Health; };\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto hover = clpp::ide::inspect(source, 3, 21);
  REQUIRE(hover.has_hover); CHECK(hover.hover.text.find("Player") != std::string::npos);
  const auto snapshot = clpp::build_snapshot(analysis);
  const auto refs = clpp::references_at(snapshot, 3, 21);
  REQUIRE(refs.size() == 2);
  for (const auto& symbol : refs) CHECK(symbol.name == "player");
  const auto line = source.substr(source.find("Reader read"));
  const auto column = static_cast<int>(line.find("player.Health") + std::string("player.").size() + 1);
  CHECK(has_label(clpp::ide::inspect(source, 3, column), "Health"));
}

TEST_CASE("editor specializes callbacks returned from open formal function references", "[formal-callback][ide]") {
  const std::string source = "def Transform<T> = (T)->T;\n"
    "template <typename T> T Identity(T value) { return value; }\n"
    "template <typename U> Transform<U> Make() { return Identity<U>; }\n"
    "def Player { number Health; }\n"
    "auto callback = Make<Player>();\n"
    "auto player = callback(Player { Health = 42 });\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto hover = clpp::ide::inspect(source, 5, 8);
  REQUIRE(hover.has_hover); INFO(hover.hover.text);
  CHECK((hover.hover.text.find("(Player)->Player") != std::string::npos ||
         hover.hover.text.find("Transform<Player>") != std::string::npos));
  const auto player = clpp::ide::inspect(source, 6, 8);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("player: Player") != std::string::npos);
  CHECK(has_label(clpp::ide::inspect(source + "player.", 8, 8), "Health"));
}

TEST_CASE("editor preserves typed async callback task and awaited properties", "[async-callback][ide]") {
  const std::string source = "def Player { number Health; }\n"
    "template <typename T> async T Identity(T value) { return value; }\n"
    "auto callback = Identity<Player>;\n"
    "auto job = callback(Player { Health = 42 });\n"
    "auto player = await job;\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto callback = clpp::ide::inspect(source, 3, 8);
  REQUIRE(callback.has_hover); CHECK(callback.hover.text.find("task<Player>") != std::string::npos);
  const auto job = clpp::ide::inspect(source, 4, 8);
  REQUIRE(job.has_hover); CHECK(job.hover.text.find("task<Player>") != std::string::npos);
  const auto player = clpp::ide::inspect(source, 5, 8);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("player: Player") != std::string::npos);
  CHECK(has_label(clpp::ide::inspect(source + "player.", 7, 8), "Health"));
}

TEST_CASE("editor resolves generic bound method signatures and awaited properties", "[method-callback][ide]") {
  const std::string source = "def Player { number Health; }\n"
    "def Box<T> { T Value; async T Read() { return @this.Value; } }\n"
    "auto box = Box<Player> { Value = Player { Health = 42 } };\n"
    "auto callback = box.Read;\n"
    "auto job = callback();\n"
    "auto player = await job;\n"
    "post(player.Health);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto callback = clpp::ide::inspect(source, 4, 8);
  REQUIRE(callback.has_hover); CHECK(callback.hover.text.find("()->task<Player>") != std::string::npos);
  const auto player = clpp::ide::inspect(source, 6, 8);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("player: Player") != std::string::npos);
  CHECK(has_label(clpp::ide::inspect(source + "player.", 8, 8), "Health"));
}

TEST_CASE("editor preserves generic default callback parameters and specialized fields", "[generic-default][ide]") {
  const std::string source = "def Transform<T> = (T)->T;\n"
    "def Box<T> { Transform<T> Apply = func(value) -> T { return value; }; array<T> Items = array<T>(); }\n"
    "auto box = Box<number> {};\n"
    "auto result = box.Apply(7);\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto parameter = clpp::ide::inspect(source, 2, 40);
  REQUIRE(parameter.has_hover); CHECK(parameter.hover.text.find("T") != std::string::npos);
  const auto result = clpp::ide::inspect(source, 4, 8);
  REQUIRE(result.has_hover); CHECK(result.hover.text.find("number") != std::string::npos);
}

TEST_CASE("editor resolves observable handles and contextual payloads", "[observable-connection][ide]") {
  const std::string source = "observable number value = 0;\n"
    "auto handle = value.OnChange(func(next) -> void { post(next); });\n"
    "handle::Disconnect();\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto handle = clpp::ide::inspect(source, 2, 8);
  REQUIRE(handle.has_hover); CHECK(handle.hover.text.find("connection") != std::string::npos);
  const auto payload = clpp::ide::inspect(source, 2, 36);
  REQUIRE(payload.has_hover); CHECK(payload.hover.text.find("number") != std::string::npos);
}

TEST_CASE("editor resolves contextual signal payloads and connection handles", "[event-subscription][ide]") {
  const std::string source = "def Player { number Health; }\n"
    "signal<Player> event;\n"
    "auto connection = event~>Connect(func(player) -> void { post(player.Health); });\n"
    "event::Fire(Player { Health = 42 });\n";
  const auto analysis = clpp::analyze_program(source, {});
  for (const auto& diagnostic : analysis.diagnostics) UNSCOPED_INFO(diagnostic.message);
  REQUIRE(analysis.ok());
  const auto handle = clpp::ide::inspect(source, 3, 8);
  REQUIRE(handle.has_hover); CHECK(handle.hover.text.find("connection") != std::string::npos);
  const auto signal = clpp::ide::inspect(source, 2, 17);
  REQUIRE(signal.has_hover); CHECK(signal.hover.text.find("signal<Player>") != std::string::npos);
  const auto player = clpp::ide::inspect(source, 3, 40);
  REQUIRE(player.has_hover); CHECK(player.hover.text.find("Player") != std::string::npos);
}
