#pragma once

#include "clpp/core/lexer/token.hpp"
#include "core/parser/ast.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace clpp::parser {

struct ParseResult {
  Program program;
  std::vector<Diagnostic> diagnostics;
};

class Parser {
 public:
  explicit Parser(std::span<const Token> tokens,
                  std::span<const Diagnostic> lexer_diagnostics = {});

  [[nodiscard]] ParseResult parse();

 private:
  [[nodiscard]] bool is_at_end() const;
  [[nodiscard]] bool check(TokenType type) const;
  [[nodiscard]] const Token& peek() const;
  [[nodiscard]] const Token& peek_at(std::size_t offset) const;
  void advance();
  [[nodiscard]] bool consume(TokenType type, std::string_view message);
  void error(std::string_view message);
  void synchronize();

  [[nodiscard]] std::optional<Stmt> parse_statement();
  [[nodiscard]] std::optional<Stmt> parse_post();
  [[nodiscard]] std::optional<Stmt> parse_let();
  [[nodiscard]] std::optional<Stmt> parse_const();
  [[nodiscard]] std::optional<Stmt> parse_auto();
  [[nodiscard]] std::optional<Stmt> parse_observable();
  [[nodiscard]] std::optional<Stmt> parse_constexpr();
  [[nodiscard]] std::optional<Stmt> parse_signal();
  [[nodiscard]] std::optional<Stmt> parse_connect();
  [[nodiscard]] std::optional<Stmt> parse_using();
  [[nodiscard]] std::optional<Stmt> parse_for_in();
  [[nodiscard]] std::optional<Stmt> parse_initialized_name(std::string declared_type);
  [[nodiscard]] std::optional<Stmt> parse_assign();
  [[nodiscard]] bool at_place_assign() const;
  [[nodiscard]] std::optional<Expr> parse_place();
  [[nodiscard]] std::optional<Stmt> parse_place_assign();
  [[nodiscard]] std::optional<Stmt> parse_compound_assign(bool require_semicolon);
  [[nodiscard]] std::optional<Stmt> parse_update(bool require_semicolon);
  [[nodiscard]] std::optional<Stmt> parse_if();
  [[nodiscard]] std::optional<Stmt> parse_while();
  [[nodiscard]] std::optional<Stmt> parse_for();
  [[nodiscard]] std::optional<Stmt> parse_break();
  [[nodiscard]] std::optional<Stmt> parse_switch();
  [[nodiscard]] std::optional<Stmt> parse_match();
  [[nodiscard]] std::optional<Stmt> parse_return();
  [[nodiscard]] std::optional<Function> parse_function();
  [[nodiscard]] std::optional<Function> parse_extern();
  [[nodiscard]] std::string parse_type_annotation();
  [[nodiscard]] std::string parse_type_atom();
  [[nodiscard]] std::string parse_generic_suffix();
  [[nodiscard]] bool starts_generic_call() const;
  [[nodiscard]] std::optional<Function> parse_void_function();
  [[nodiscard]] std::optional<StructDecl> parse_struct();
  [[nodiscard]] std::optional<EnumDecl> parse_enum();
  [[nodiscard]] std::optional<VariantDecl> parse_variant();
  [[nodiscard]] bool parse_link(Program& program);
  [[nodiscard]] bool parse_parameter_list(Function& function);
  [[nodiscard]] std::optional<std::vector<Stmt>> parse_block();
  [[nodiscard]] std::optional<Expr> parse_expression();
  [[nodiscard]] std::optional<Expr> parse_or();
  [[nodiscard]] std::optional<Expr> parse_and();
  [[nodiscard]] std::optional<Expr> parse_bitor();
  [[nodiscard]] std::optional<Expr> parse_bitxor();
  [[nodiscard]] std::optional<Expr> parse_bitand();
  [[nodiscard]] std::optional<Expr> parse_shift();
  [[nodiscard]] std::optional<Stmt> parse_try();
  [[nodiscard]] std::optional<Expr> parse_unary();
  [[nodiscard]] std::optional<Expr> parse_comparison();
  [[nodiscard]] std::optional<Expr> parse_additive();
  [[nodiscard]] std::optional<Expr> parse_multiplicative();
  [[nodiscard]] std::optional<Expr> parse_primary();
  [[nodiscard]] std::optional<Expr> parse_self_field();
  [[nodiscard]] std::optional<Expr> parse_template();
  [[nodiscard]] std::optional<Expr> parse_embedded_expression();
  [[nodiscard]] std::optional<Expr> finish_postfix(Expr expr);

  Program* m_program{nullptr};
  std::span<const Token> m_tokens;
  std::span<const Diagnostic> m_lexer_diagnostics;
  std::size_t m_current{0};
  int m_block_depth{0};
  std::vector<Diagnostic> m_diagnostics;
};

}  // namespace clpp::parser
