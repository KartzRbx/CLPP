#pragma once

#include "clpp/core/lexer/token.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

// Every reserved word of the language, from the lexer's own table (the IDE lists these).
[[nodiscard]] std::vector<std::string_view> keyword_names();

class Lexer {
 public:
  // The lexer never copies the source: tokens and diagnostics are views into it, so the
  // buffer must outlive the LexResult. A temporary std::string would dangle, so it is rejected.
  explicit Lexer(std::string_view source);
  explicit Lexer(const std::string& source) : Lexer(std::string_view(source)) {}
  explicit Lexer(std::string&& source) = delete;
  explicit Lexer(const char* source) : Lexer(std::string_view(source)) {}

  [[nodiscard]] LexResult tokenize();

 private:
  struct CursorState {
    std::size_t cursor{0};
    std::uint32_t line{1};
    std::uint32_t column{1};
  };

  [[nodiscard]] char peek() const;
  [[nodiscard]] char peek_next() const;
  [[nodiscard]] char peek_at(std::size_t offset) const;
  char advance();
  [[nodiscard]] bool match(char expected);
  [[nodiscard]] bool is_at_end() const;
  [[nodiscard]] bool at_line_end() const;
  [[nodiscard]] std::string_view slice(std::size_t start) const;

  [[nodiscard]] CursorState save_state() const;
  void restore_state(CursorState state);
  void report(SourceLocation loc, std::string_view span, std::string_view message);

  void skip_whitespace();
  void skip_line_comment_after_ll();
  [[nodiscard]] bool skip_block_comment_after_lbracket();
  void consume_less_less_comment(SourceLocation start_loc, std::size_t comment_start,
                                 std::vector<Token>& tokens);

  [[nodiscard]] Token make_token(TokenType type, std::string_view text, SourceLocation loc);
  [[nodiscard]] bool take_utf8();
  [[nodiscard]] Token read_identifier_or_keyword();
  [[nodiscard]] Token read_number();
  [[nodiscard]] Token read_quoted_string(char quote, SourceLocation open_loc, std::size_t open_index);
  [[nodiscard]] Token read_template_string(SourceLocation open_loc, std::size_t open_index);
  void read_template(SourceLocation open_loc, std::size_t open_index, std::vector<Token>& tokens);
  [[nodiscard]] bool skip_balanced_braces();
  [[nodiscard]] bool skip_template_body();

  std::string_view m_source;
  std::size_t m_cursor{0};
  std::uint32_t m_line{1};
  std::uint32_t m_column{1};
  std::vector<Diagnostic> m_diagnostics;
};

}  // namespace clpp
