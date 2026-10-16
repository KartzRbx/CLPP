#include "clpp/core/lexer/lexer.hpp"
#include "core/parser/parser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

namespace {

[[nodiscard]] clpp::parser::ParseResult parse_source(const std::string_view source) {
  clpp::Lexer lexer(source);
  const clpp::LexResult lexed = lexer.tokenize();
  clpp::parser::Parser parser(lexed.tokens, lexed.diagnostics);
  return parser.parse();
}

}  // namespace

TEST_CASE("parser accepts empty token stream", "[parser]") {
  const clpp::Token eof{.type = clpp::TokenType::Eof, .lexeme = {}};
  const std::array tokens{eof};
  clpp::parser::Parser parser(tokens);
  const clpp::parser::ParseResult result = parser.parse();
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.empty());
}

TEST_CASE("parser post string statement", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(\"hello\");");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.statements.size() == 1);
  const clpp::parser::Expr& expr = result.program.statements[0].expr;
  REQUIRE(expr.kind == clpp::parser::Expr::Kind::String);
  REQUIRE(expr.value == "hello");
}

TEST_CASE("parser single quoted string", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post('hi');");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.statements[0].expr.value == "hi");
}

TEST_CASE("parser concat is left associative", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(\"a\" .: \"b\" .: \"c\");");
  REQUIRE(result.diagnostics.empty());
  const clpp::parser::Expr& expr = result.program.statements[0].expr;
  REQUIRE(expr.kind == clpp::parser::Expr::Kind::Concat);
  REQUIRE(expr.left != nullptr);
  REQUIRE(expr.left->kind == clpp::parser::Expr::Kind::Concat);
  REQUIRE(expr.left->left->value == "a");
  REQUIRE(expr.left->right->value == "b");
  REQUIRE(expr.right->value == "c");
}

TEST_CASE("parser missing closing paren", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(\"hello\";");
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "expected ')'");
}

TEST_CASE("parser missing semicolon", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(\"hello\")");
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "expected ';'");
}

TEST_CASE("parser rejects a misplaced else statement", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("else work;");
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "not in this subset");
}

TEST_CASE("parser keeps lexer diagnostic and skips the statement", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("\"hello");
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "unterminated string");
}

TEST_CASE("parser multiplication binds tighter than addition", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(1 + 2 * 3);");
  REQUIRE(result.diagnostics.empty());
  const clpp::parser::Expr& expr = result.program.statements[0].expr;
  REQUIRE(expr.kind == clpp::parser::Expr::Kind::Add);
  REQUIRE(expr.left->kind == clpp::parser::Expr::Kind::Number);
  REQUIRE(expr.left->number == 1);
  REQUIRE(expr.right->kind == clpp::parser::Expr::Kind::Mul);
  REQUIRE(expr.right->left->number == 2);
  REQUIRE(expr.right->right->number == 3);
}

TEST_CASE("parser parentheses override precedence", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post((1 + 2) * 3);");
  REQUIRE(result.diagnostics.empty());
  const clpp::parser::Expr& expr = result.program.statements[0].expr;
  REQUIRE(expr.kind == clpp::parser::Expr::Kind::Mul);
  REQUIRE(expr.left->kind == clpp::parser::Expr::Kind::Add);
  REQUIRE(expr.left->left->number == 1);
  REQUIRE(expr.left->right->number == 2);
  REQUIRE(expr.right->number == 3);
}

TEST_CASE("parser incomplete addition reports expected expression", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(1 +);");
  REQUIRE(result.program.statements.empty());
  REQUIRE(result.diagnostics.size() == 1);
  REQUIRE(result.diagnostics[0].message == "expected expression");
}

TEST_CASE("parser inferred constant and name use", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("const auto x = 1 + 2; post(x);");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.statements.size() == 2);
  REQUIRE(result.program.statements[0].kind == clpp::parser::Stmt::Kind::VariableDecl);
  REQUIRE(result.program.statements[0].name == "x");
  REQUIRE(result.program.statements[0].expr.kind == clpp::parser::Expr::Kind::Add);
  REQUIRE(result.program.statements[1].kind == clpp::parser::Stmt::Kind::Post);
  REQUIRE(result.program.statements[1].expr.kind == clpp::parser::Expr::Kind::Name);
  REQUIRE(result.program.statements[1].expr.value == "x");
}

TEST_CASE("parser two post statements", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("post(\"hello\");\npost(\"a\" .: \"b\");");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.statements.size() == 2);
  REQUIRE(result.program.statements[0].expr.value == "hello");
  REQUIRE(result.program.statements[1].expr.kind == clpp::parser::Expr::Kind::Concat);
}

TEST_CASE("pragma selects a checking mode only at file start", "[parser][pragma]") {
  auto strict = parse_source("pragma strict; number hp = 100;");
  REQUIRE(strict.diagnostics.empty());
  REQUIRE(strict.program.mode == clpp::parser::CheckMode::Strict);
  auto loose = parse_source("pragma nostrict; number hp = 100;");
  REQUIRE(loose.diagnostics.empty());
  REQUIRE(loose.program.mode == clpp::parser::CheckMode::Nonstrict);
  REQUIRE_FALSE(parse_source("number hp = 100; pragma strict;").diagnostics.empty());
  REQUIRE_FALSE(parse_source("pragma strict; pragma nostrict;").diagnostics.empty());
  REQUIRE_FALSE(parse_source("pragma unknown;").diagnostics.empty());
  REQUIRE_FALSE(parse_source("pragma strict number hp = 100;").diagnostics.empty());
}

TEST_CASE("authored comments are source trivia and C++ comments are rejected", "[parser][syntax]") {
  REQUIRE(parse_source("<< line comment\npost(1);").diagnostics.empty());
  REQUIRE(parse_source("<<[ block comment ]>>\npost(1);").diagnostics.empty());
  const auto comment = parse_source("<<!strict\npost(1);");
  REQUIRE(comment.diagnostics.empty());
  REQUIRE(comment.program.mode == clpp::parser::CheckMode::Nonstrict);
  REQUIRE_FALSE(parse_source("// not a CL++ comment\npost(1);").diagnostics.empty());
  REQUIRE_FALSE(parse_source("/* not a CL++ comment */\npost(1);").diagnostics.empty());
}

TEST_CASE("excessive syntax nesting produces a diagnostic rather than native stack overflow", "[parser][limits]") {
  for (const std::string& source : {
      "post(" + std::string(128, '(') + "1" + std::string(128, ')') + ");",
      "post(" + std::string(256, '!') + "true);",
      [] { std::string code; for(int i=0;i<256;++i) code+="if (true) {"; return code + "post(1);" + std::string(256, '}'); }(),
      [] { std::string type="number"; for(int i=0;i<128;++i) type="array<"+type+">"; return type + " value = {};"; }()
  }) {
    const auto parsed = parse_source(source);
    bool limited = false;
    for (const auto& diagnostic : parsed.diagnostics) limited |= diagnostic.message == "syntax nesting limit exceeded";
    INFO(source.substr(0, 100)); REQUIRE(limited);
  }
  const auto accepted = parse_source("post(" + std::string(64, '(') + "1" + std::string(64, ')') + ");");
  REQUIRE(accepted.diagnostics.empty());
}

TEST_CASE("iterative operator and member chains have bounded AST depth", "[parser][limits]") {
  for (const bool members : {false, true}) {
    std::string expression = members ? "value" : "1";
    for (int index = 0; index < 512; ++index) expression += members ? ".Field" : "+1";
    const auto parsed = parse_source("post(" + expression + ");");
    bool limited = false;
    for (const auto& diagnostic : parsed.diagnostics) limited |= diagnostic.message == "syntax nesting limit exceeded";
    REQUIRE(limited);
  }
  std::string accepted = "1";
  for (int index = 1; index < 128; ++index) accepted += "+1";
  REQUIRE(parse_source("post(" + accepted + ");").diagnostics.empty());
}

TEST_CASE("export marks canonical module declarations", "[parser][export]") {
  const auto result = parse_source(
      "export def Player { number Health = 100; } def Internal { number Value; }\n"
      "export def State = \"idle\" | \"running\"; def Hidden = number;\n"
      "export enum class Mode { Start, Stop } enum class PrivateMode { One }\n"
      "export number Add(number a, number b) { return a + b; } number Helper() { return 1; }\n"
      "export async number Work() { return 2; }\n"
      "export const number LIMIT = 100; export const auto INFERRED = 5; const number PRIVATE = 3;");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.structs.size() == 2);
  REQUIRE(result.program.structs[0].exported);
  REQUIRE_FALSE(result.program.structs[1].exported);
  REQUIRE(result.program.aliases[0].exported);
  REQUIRE_FALSE(result.program.aliases[1].exported);
  REQUIRE(result.program.enums[0].exported);
  REQUIRE_FALSE(result.program.enums[1].exported);
  REQUIRE(result.program.functions[0].exported);
  REQUIRE_FALSE(result.program.functions[1].exported);
  REQUIRE(result.program.functions[2].exported);
  REQUIRE(result.program.functions[2].is_async);
  REQUIRE(result.program.statements[0].exported);
  REQUIRE(result.program.statements[1].exported);
  REQUIRE_FALSE(result.program.statements[2].exported);
}

TEST_CASE("export rejects executable statements and obsolete declarations", "[parser][export]") {
  for (const char* source : {"export number value = 1;", "export auto value = 1;",
       "export post(1);", "export link \"./other.clp\" as Other;",
       "export export const number LIMIT = 3;", "export func Old() -> number { return 1; }",
       "export struct Old { number Value; }", "export type Old = number;",
       "export async func Old() -> number { return 1; }",
       "export enum Old { One }", "void F() { export const number LIMIT = 1; }"}) {
    INFO(source);
    REQUIRE_FALSE(parse_source(source).diagnostics.empty());
  }
}

TEST_CASE("parser preserves module qualified type annotations", "[parser][modules]") {
  const auto result = parse_source(
      "Data.Player Read(optional<Data.Player> player) { return player; }\n"
      "def Wrapper { array<Data.Player> Players; }\n"
      "def Public = Data.Player; Data.Player value = null;");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.functions[0].return_type == "Data.Player");
  REQUIRE(result.program.functions[0].param_types[0] == "optional<Data.Player>");
  REQUIRE(result.program.structs[0].fields[0].type_name == "array<Data.Player>");
  REQUIRE(result.program.aliases[0].body == "Data.Player");
  REQUIRE(result.program.statements[0].declared_type == "Data.Player");
}
