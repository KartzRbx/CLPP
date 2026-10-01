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

TEST_CASE("parser rejects tokens outside the post subset", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("spawn work;");
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

TEST_CASE("parser let and name use", "[parser]") {
  const clpp::parser::ParseResult result = parse_source("let x = 1 + 2; post(x);");
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.program.statements.size() == 2);
  REQUIRE(result.program.statements[0].kind == clpp::parser::Stmt::Kind::Let);
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
