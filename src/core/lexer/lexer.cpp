#include "clpp/core/lexer/lexer.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace clpp {

namespace {

struct KeywordEntry {
  std::string_view kw;
  TokenType type;
};

// Sorted by kw for std::lower_bound (no hash per identifier).
constexpr std::array kKeywords = {
    KeywordEntry{"Vector2", TokenType::KwVector2},
    KeywordEntry{"Vector3", TokenType::KwVector3},
    KeywordEntry{"Vector4", TokenType::KwVector4},
    KeywordEntry{"abstract", TokenType::KwAbstract},
    KeywordEntry{"and", TokenType::KwAnd},
    KeywordEntry{"as", TokenType::KwAs},
    KeywordEntry{"async", TokenType::KwAsync},
    KeywordEntry{"atomic", TokenType::KwAtomic},
    KeywordEntry{"auto", TokenType::KwAuto},
    KeywordEntry{"await", TokenType::KwAwait},
    KeywordEntry{"bool", TokenType::KwBool},
    KeywordEntry{"break", TokenType::KwBreak},
    KeywordEntry{"buffer", TokenType::KwBuffer},
    KeywordEntry{"case", TokenType::KwCase},
    KeywordEntry{"catch", TokenType::KwCatch},
    KeywordEntry{"const", TokenType::KwConst},
    KeywordEntry{"constexpr", TokenType::KwConstexpr},
    KeywordEntry{"continue", TokenType::KwContinue},
    KeywordEntry{"cout", TokenType::KwCout},
    KeywordEntry{"decltype", TokenType::KwDecltype},
    KeywordEntry{"default", TokenType::KwDefault},
    KeywordEntry{"double", TokenType::KwDouble},
    KeywordEntry{"else", TokenType::KwElse},
    KeywordEntry{"endl", TokenType::KwEndl},
    KeywordEntry{"enum", TokenType::KwEnum},
    KeywordEntry{"extern", TokenType::KwExtern},
    KeywordEntry{"false", TokenType::KwFalse},
    KeywordEntry{"final", TokenType::KwFinal},
    KeywordEntry{"float", TokenType::KwFloat},
    KeywordEntry{"for", TokenType::KwFor},
    KeywordEntry{"from", TokenType::KwFrom},
    KeywordEntry{"func", TokenType::KwFunc},
    KeywordEntry{"guard", TokenType::KwGuard},
    KeywordEntry{"if", TokenType::KwIf},
    KeywordEntry{"import", TokenType::KwImport},
    KeywordEntry{"in", TokenType::KwIn},
    KeywordEntry{"int", TokenType::KwInt},
    KeywordEntry{"join", TokenType::KwJoin},
    KeywordEntry{"let", TokenType::KwLet},
    KeywordEntry{"link", TokenType::KwLink},
    KeywordEntry{"match", TokenType::KwMatch},
    KeywordEntry{"move", TokenType::KwMove},
    KeywordEntry{"mut", TokenType::KwMut},
    KeywordEntry{"namespace", TokenType::KwNamespace},
    KeywordEntry{"new", TokenType::KwNew},
    KeywordEntry{"not", TokenType::KwNot},
    KeywordEntry{"null", TokenType::KwNull},
    KeywordEntry{"observable", TokenType::KwObservable},
    KeywordEntry{"operator", TokenType::KwOperator},
    KeywordEntry{"or", TokenType::KwOr},
    KeywordEntry{"override", TokenType::KwOverride},
    KeywordEntry{"parallel", TokenType::KwParallel},
    KeywordEntry{"pcall", TokenType::KwPcall},
    KeywordEntry{"post", TokenType::KwPost},
    KeywordEntry{"private", TokenType::KwPrivate},
    KeywordEntry{"public", TokenType::KwPublic},
    KeywordEntry{"report", TokenType::KwReport},
    KeywordEntry{"return", TokenType::KwReturn},
    KeywordEntry{"shl", TokenType::KwShl},
    KeywordEntry{"shr", TokenType::KwShr},
    KeywordEntry{"signal", TokenType::KwSignal},
    KeywordEntry{"spawn", TokenType::KwSpawn},
    KeywordEntry{"static", TokenType::KwStatic},
    KeywordEntry{"string", TokenType::KwString},
    KeywordEntry{"struct", TokenType::KwStruct},
    KeywordEntry{"switch", TokenType::KwSwitch},
    KeywordEntry{"task", TokenType::KwTask},
    KeywordEntry{"thread", TokenType::KwThread},
    KeywordEntry{"true", TokenType::KwTrue},
    KeywordEntry{"try", TokenType::KwTry},
    KeywordEntry{"type", TokenType::KwType},
    KeywordEntry{"using", TokenType::KwUsing},
    KeywordEntry{"variant", TokenType::KwVariant},
    KeywordEntry{"void", TokenType::KwVoid},
    KeywordEntry{"warn", TokenType::KwWarn},
    KeywordEntry{"where", TokenType::KwWhere},
    KeywordEntry{"while", TokenType::KwWhile},
};

constexpr std::string_view kUnterminatedString = "unterminated string";
constexpr std::string_view kUnterminatedTemplate = "unterminated template string";
constexpr std::string_view kUnterminatedBlockComment = "unterminated block comment";
constexpr std::string_view kUnterminatedDocComment = "unterminated doc comment";
constexpr std::string_view kInvalidSeparator = "invalid numeric separator";
constexpr std::string_view kInvalidLiteral = "invalid numeric literal";
constexpr std::string_view kUseAnd = "use 'and' instead of '&&'";
constexpr std::string_view kUseOr = "use 'or' instead of '||'";
constexpr std::string_view kUnexpectedCharacter = "unexpected character";
constexpr std::string_view kControlCharacter = "control character";

constexpr bool keywords_are_sorted() {
  for (std::size_t i = 1; i < kKeywords.size(); ++i) {
    if (kKeywords[i - 1].kw >= kKeywords[i].kw) {
      return false;
    }
  }
  return true;
}
static_assert(keywords_are_sorted(), "kKeywords must stay lexicographically sorted");

[[nodiscard]] bool is_identifier_start(char c) {
  return static_cast<unsigned char>(c) == '_' ||
         std::isalpha(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] bool is_identifier_continue(char c) {
  return is_identifier_start(c) ||
         std::isdigit(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] TokenType lookup_keyword(std::string_view text) {
  const auto it = std::lower_bound(
      kKeywords.begin(), kKeywords.end(), text,
      [](const KeywordEntry& entry, std::string_view key) { return entry.kw < key; });
  if (it != kKeywords.end() && it->kw == text) {
    return it->type;
  }
  return TokenType::Identifier;
}

[[nodiscard]] bool is_hex_body_char(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0 || c == '_';
}

[[nodiscard]] bool is_bin_body_char(char c) {
  return c == '0' || c == '1' || c == '_';
}

[[nodiscard]] bool is_dec_body_char(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0 || c == '_';
}

[[nodiscard]] bool is_digit_for_base(char c, int base) {
  if (base == 16) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
  }
  if (base == 2) {
    return c == '0' || c == '1';
  }
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

enum class ComponentStatus { Ok, BadSeparator, MissingDigit };

// '_' is allowed only between two digits of the same component.
[[nodiscard]] ComponentStatus check_numeric_component(std::string_view body, int base) {
  if (body.empty()) {
    return ComponentStatus::MissingDigit;
  }
  if (body.front() == '_' || body.back() == '_') {
    return ComponentStatus::BadSeparator;
  }

  bool prev_sep = false;
  bool saw_digit = false;
  for (const char c : body) {
    if (c == '_') {
      if (prev_sep || !saw_digit) {
        return ComponentStatus::BadSeparator;
      }
      prev_sep = true;
      continue;
    }
    if (!is_digit_for_base(c, base)) {
      return ComponentStatus::BadSeparator;
    }
    prev_sep = false;
    saw_digit = true;
  }

  if (prev_sep || !saw_digit) {
    return ComponentStatus::BadSeparator;
  }
  return ComponentStatus::Ok;
}

[[nodiscard]] std::string_view component_message(ComponentStatus status) {
  if (status == ComponentStatus::BadSeparator) {
    return kInvalidSeparator;
  }
  return kInvalidLiteral;
}

}  // namespace

std::vector<std::string_view> keyword_names() {
  std::vector<std::string_view> names;
  names.reserve(kKeywords.size());
  for (const KeywordEntry& entry : kKeywords) {
    names.push_back(entry.kw);
  }
  return names;
}

Lexer::Lexer(std::string_view source) : m_source(source) {}

LexResult Lexer::tokenize() {
  m_diagnostics.clear();
  std::vector<Token> tokens;
  if (m_source.size() >= 3 && static_cast<unsigned char>(m_source[0]) == 0xEF &&
      static_cast<unsigned char>(m_source[1]) == 0xBB && static_cast<unsigned char>(m_source[2]) == 0xBF) {
    m_cursor = 3;
    m_column = 1;
  }

  while (!is_at_end()) {
    skip_whitespace();
    if (is_at_end()) {
      break;
    }

    const std::size_t start = m_cursor;
    const SourceLocation start_loc{m_line, m_column};

    if (peek() == '<' && peek_next() == '<') {
      advance();
      advance();
      consume_less_less_comment(start_loc, start, tokens);
      continue;
    }

    if (is_identifier_start(peek()) || static_cast<unsigned char>(peek()) >= 0x80) {
      if (static_cast<unsigned char>(peek()) >= 0x80 && !take_utf8()) {
        advance();
        const std::string_view text = slice(start);
        report(start_loc, text, "invalid utf-8");
        tokens.push_back(make_token(TokenType::Invalid, text, start_loc));
        continue;
      }
      if (static_cast<unsigned char>(m_source[start]) >= 0x80) {
        restore_state(CursorState{start, start_loc.line, start_loc.column});
      }
      tokens.push_back(read_identifier_or_keyword());
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
      tokens.push_back(read_number());
      continue;
    }

    if (peek() == '"' || peek() == '\'') {
      const char quote = advance();
      tokens.push_back(read_quoted_string(quote, start_loc, start));
      continue;
    }

    if (peek() == '`') {
      advance();
      read_template(start_loc, start, tokens);
      continue;
    }

    const auto control = static_cast<unsigned char>(peek());
    if (control < 0x20 || control == 0x7F) {
      advance();
      const std::string_view text = slice(start);
      report(start_loc, text, kControlCharacter);
      tokens.push_back(make_token(TokenType::Invalid, text, start_loc));
      continue;
    }

    const char c = advance();
    const auto push_span = [&](TokenType type) {
      tokens.push_back(make_token(type, slice(start), start_loc));
    };
    const auto push_invalid = [&](std::string_view message) {
      const std::string_view text = slice(start);
      report(start_loc, text, message);
      tokens.push_back(make_token(TokenType::Invalid, text, start_loc));
    };

    switch (c) {
      case '(': push_span(TokenType::OpenParen); break;
      case ')': push_span(TokenType::CloseParen); break;
      case '{': push_span(TokenType::OpenBrace); break;
      case '}': push_span(TokenType::CloseBrace); break;
      case '[': push_span(TokenType::OpenBracket); break;
      case ']': push_span(TokenType::CloseBracket); break;
      case ';': push_span(TokenType::Semicolon); break;
      case ',': push_span(TokenType::Comma); break;
      case '@': push_span(TokenType::At); break;
      case '#': push_span(TokenType::Hash); break;

      case '+':
        if (match('+')) {
          push_span(TokenType::PlusPlus);
        } else if (match('=')) {
          push_span(TokenType::PlusEq);
        } else {
          push_span(TokenType::Plus);
        }
        break;

      case '-':
        if (match('-')) {
          push_span(TokenType::MinusMinus);
        } else if (match('>')) {
          push_span(TokenType::Arrow);
        } else if (match('=')) {
          push_span(TokenType::MinusEq);
        } else {
          push_span(TokenType::Minus);
        }
        break;

      case '*':
        if (match('=')) {
          push_span(TokenType::StarEq);
        } else {
          push_span(TokenType::Star);
        }
        break;

      case '/':
        if (match('=')) {
          push_span(TokenType::SlashEq);
        } else {
          push_span(TokenType::Slash);
        }
        break;

      case '%':
        if (match('=')) {
          push_span(TokenType::PercentEq);
        } else {
          push_span(TokenType::Percent);
        }
        break;

      case '=':
        if (match('=')) {
          push_span(TokenType::EqualEqual);
        } else if (match('>')) {
          push_span(TokenType::FatArrow);
        } else {
          push_span(TokenType::Equal);
        }
        break;

      case '!':
        if (match('=')) {
          push_span(TokenType::NotEqual);
        } else {
          push_span(TokenType::Bang);
        }
        break;

      case '?':
        push_span(TokenType::Question);
        break;

      case '.':
        if (match(':')) {
          push_span(TokenType::Concat);
        } else if (match('.')) {
          if (match('.')) {
            push_span(TokenType::Ellipsis);
          } else {
            push_span(TokenType::DotDot);
          }
        } else {
          push_span(TokenType::Dot);
        }
        break;

      case ':':
        if (match(':')) {
          push_span(TokenType::Scope);
        } else {
          push_span(TokenType::Colon);
        }
        break;

      case '~':
        if (match('>')) {
          push_span(TokenType::SignalConnect);
        } else {
          push_span(TokenType::BitNot);
        }
        break;

      case '^':
        push_span(TokenType::Caret);
        break;

      case '<':
        if (match('=')) {
          push_span(TokenType::LessEqual);
        } else {
          push_span(TokenType::Less);
        }
        break;

      case '>':
        if (match('=')) {
          push_span(TokenType::GreaterEqual);
        } else {
          push_span(TokenType::Greater);
        }
        break;

      case '&':
        if (match('&')) {
          push_span(TokenType::KwAnd);
        } else {
          push_span(TokenType::Ampersand);
        }
        break;

      case '|':
        if (match('|')) {
          push_span(TokenType::KwOr);
        } else {
          push_span(TokenType::Pipe);
        }
        break;

      default: {
        const auto lead = static_cast<unsigned char>(c);
        if (lead >= 0x80) {
          int width = 1;
          if ((lead & 0xE0) == 0xC0) {
            width = 2;
          } else if ((lead & 0xF0) == 0xE0) {
            width = 3;
          } else if ((lead & 0xF8) == 0xF0) {
            width = 4;
          }
          for (int index = 1; index < width && !is_at_end(); ++index) {
            const auto next = static_cast<unsigned char>(peek());
            if ((next & 0xC0) != 0x80) {
              break;
            }
            advance();
          }
          push_invalid("invalid utf-8");
        } else {
          push_invalid(kUnexpectedCharacter);
        }
        break;
      }
    }
  }

  tokens.push_back(make_token(TokenType::Eof, "", SourceLocation{m_line, m_column}));
  LexResult result;
  result.tokens = std::move(tokens);
  result.diagnostics = std::move(m_diagnostics);
  return result;
}

char Lexer::peek() const {
  if (is_at_end()) {
    return '\0';
  }
  return m_source[m_cursor];
}

char Lexer::peek_next() const {
  return peek_at(1);
}

char Lexer::peek_at(const std::size_t offset) const {
  if (m_cursor + offset >= m_source.size()) {
    return '\0';
  }
  return m_source[m_cursor + offset];
}

char Lexer::advance() {
  const char c = m_source[m_cursor++];
  if (c == '\r') {
    if (!is_at_end() && m_source[m_cursor] == '\n') {
      ++m_cursor;
    }
    ++m_line;
    m_column = 1;
  } else if (c == '\n') {
    ++m_line;
    m_column = 1;
  } else {
    ++m_column;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (is_at_end() || m_source[m_cursor] != expected) {
    return false;
  }
  advance();
  return true;
}

bool Lexer::is_at_end() const {
  return m_cursor >= m_source.size();
}

// End of a physical line: LF, CR or CRLF all end a line comment, a string or a template.
bool Lexer::at_line_end() const {
  return is_at_end() || m_source[m_cursor] == '\n' || m_source[m_cursor] == '\r';
}

std::string_view Lexer::slice(const std::size_t start) const {
  return m_source.substr(start, m_cursor - start);
}

Lexer::CursorState Lexer::save_state() const {
  return CursorState{m_cursor, m_line, m_column};
}

void Lexer::restore_state(const CursorState state) {
  m_cursor = state.cursor;
  m_line = state.line;
  m_column = state.column;
}

void Lexer::report(const SourceLocation loc, const std::string_view span,
                   const std::string_view message) {
  m_diagnostics.push_back(Diagnostic{loc, span, std::string(message)});
}

void Lexer::skip_whitespace() {
  while (!is_at_end()) {
    const char c = peek();
    if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
      advance();
    } else {
      break;
    }
  }
}

void Lexer::skip_line_comment_after_ll() {
  while (!at_line_end()) {
    advance();
  }
}

bool Lexer::skip_block_comment_after_lbracket() {
  while (!is_at_end()) {
    if (peek() == ']' && peek_next() == '>' && peek_at(2) == '>') {
      advance();
      advance();
      advance();
      return true;
    }
    advance();
  }
  return false;
}

void Lexer::consume_less_less_comment(const SourceLocation start_loc, const std::size_t comment_start,
                                      std::vector<Token>& tokens) {
  const CursorState after_ll = save_state();

  // Doc opener after <<: <<[[]] or <<[[]]] (optional second ] before body)
  if (peek_at(0) == '[' && peek_at(1) == '[' && peek_at(2) == ']') {
    std::size_t opener_len = 3;
    if (peek_at(3) == ']') {
      opener_len = 4;
    }
    for (std::size_t i = 0; i < opener_len; ++i) {
      advance();
    }

    const std::size_t content_start = m_cursor;
    while (!is_at_end()) {
      if (peek() == '>' && peek_next() == '>') {
        const std::string_view text = m_source.substr(content_start, m_cursor - content_start);
        tokens.push_back(make_token(TokenType::DocComment, text, start_loc));
        advance();
        advance();
        return;
      }
      advance();
    }

    const std::string_view span = m_source.substr(comment_start, m_cursor - comment_start);
    report(start_loc, span, kUnterminatedDocComment);
    tokens.push_back(make_token(TokenType::Invalid, span, start_loc));
    return;
  }

  restore_state(after_ll);

  if (peek() == '[') {
    advance();
    if (!skip_block_comment_after_lbracket()) {
      const std::string_view span = m_source.substr(comment_start, m_cursor - comment_start);
      report(start_loc, span, kUnterminatedBlockComment);
    }
    return;
  }

  if (peek() == '!') {
    const CursorState saved = save_state();
    advance();
    const std::size_t word_start = m_cursor;
    while (!is_at_end() && is_identifier_continue(peek())) {
      advance();
    }
    const std::string_view word = m_source.substr(word_start, m_cursor - word_start);
    if (word == "strict" || word == "nonstrict" || word == "nocheck") {
      tokens.push_back(make_token(TokenType::Mode, word, start_loc));
      skip_line_comment_after_ll();
      return;
    }
    restore_state(saved);
  }

  skip_line_comment_after_ll();
}

Token Lexer::make_token(const TokenType type, const std::string_view text, const SourceLocation loc) {
  return Token{type, text, loc};
}

bool Lexer::take_utf8() {
  if (is_at_end()) {
    return false;
  }
  const auto lead = static_cast<unsigned char>(peek());
  int width = 1;
  if ((lead & 0xE0) == 0xC0) {
    width = 2;
  } else if ((lead & 0xF0) == 0xE0) {
    width = 3;
  } else if ((lead & 0xF8) == 0xF0) {
    width = 4;
  }
  if (lead < 0x80 || width == 1) {
    return false;
  }
  if (m_cursor + static_cast<std::size_t>(width) > m_source.size()) {
    return false;
  }
  for (int index = 1; index < width; ++index) {
    const auto next = static_cast<unsigned char>(m_source[m_cursor + static_cast<std::size_t>(index)]);
    if ((next & 0xC0) != 0x80) {
      return false;
    }
  }
  for (int index = 0; index < width; ++index) {
    advance();
  }
  return true;
}

Token Lexer::read_identifier_or_keyword() {
  const SourceLocation loc{m_line, m_column};
  const std::size_t start = m_cursor;

  while (!is_at_end()) {
    if (is_identifier_continue(peek())) {
      advance();
      continue;
    }
    if (!take_utf8()) {
      break;
    }
  }

  const std::string_view text = m_source.substr(start, m_cursor - start);
  return make_token(lookup_keyword(text), text, loc);
}

Token Lexer::read_number() {
  const SourceLocation loc{m_line, m_column};
  const std::size_t start = m_cursor;

  const auto fail = [&](std::string_view message) {
    const std::string_view text = slice(start);
    report(loc, text, message);
    return make_token(TokenType::Invalid, text, loc);
  };

  if (peek() == '0' &&
      (peek_next() == 'x' || peek_next() == 'X' || peek_next() == 'b' || peek_next() == 'B')) {
    const char base_char = static_cast<char>(std::tolower(static_cast<unsigned char>(peek_next())));
    advance();
    advance();

    const std::size_t body_start = m_cursor;
    if (base_char == 'x') {
      while (!is_at_end() && is_hex_body_char(peek())) {
        advance();
      }
    } else {
      while (!is_at_end() && is_bin_body_char(peek())) {
        advance();
      }
    }

    const std::string_view body = m_source.substr(body_start, m_cursor - body_start);
    const int base = base_char == 'x' ? 16 : 2;
    const ComponentStatus status = check_numeric_component(body, base);
    if (status != ComponentStatus::Ok) {
      return fail(component_message(status));
    }

    return make_token(TokenType::IntLiteral, slice(start), loc);
  }

  const std::size_t int_start = m_cursor;
  while (!is_at_end() && is_dec_body_char(peek())) {
    advance();
  }
  const std::string_view int_body = m_source.substr(int_start, m_cursor - int_start);

  bool is_float = false;
  bool has_fraction = false;
  std::string_view frac_body;
  if (!is_at_end() && peek() == '.' &&
      (std::isdigit(static_cast<unsigned char>(peek_next())) != 0 || peek_next() == '_')) {
    is_float = true;
    has_fraction = true;
    advance();
    const std::size_t frac_start = m_cursor;
    while (!is_at_end() && is_dec_body_char(peek())) {
      advance();
    }
    frac_body = m_source.substr(frac_start, m_cursor - frac_start);
  }

  bool has_exponent = false;
  std::string_view exp_body;
  if (!is_at_end() && (peek() == 'e' || peek() == 'E')) {
    is_float = true;
    has_exponent = true;
    advance();
    if (!is_at_end() && (peek() == '+' || peek() == '-')) {
      advance();
    }
    const std::size_t exp_start = m_cursor;
    while (!is_at_end() && is_dec_body_char(peek())) {
      advance();
    }
    exp_body = m_source.substr(exp_start, m_cursor - exp_start);
  }

  if (const ComponentStatus status = check_numeric_component(int_body, 10); status != ComponentStatus::Ok) {
    return fail(component_message(status));
  }
  if (has_fraction) {
    if (const ComponentStatus status = check_numeric_component(frac_body, 10); status != ComponentStatus::Ok) {
      return fail(component_message(status));
    }
  }
  if (has_exponent) {
    if (const ComponentStatus status = check_numeric_component(exp_body, 10); status != ComponentStatus::Ok) {
      return fail(component_message(status));
    }
  }

  return make_token(is_float ? TokenType::FloatLiteral : TokenType::IntLiteral, slice(start), loc);
}

Token Lexer::read_quoted_string(const char quote, const SourceLocation open_loc,
                                const std::size_t open_index) {
  const std::size_t content_start = m_cursor;

  while (!is_at_end()) {
    if (peek() == '\n' || peek() == '\r') {
      break;
    }
    if (peek() == '\\') {
      advance();
      if (!at_line_end()) {
        advance();
      }
      continue;
    }
    if (peek() == quote) {
      const std::string_view text = m_source.substr(content_start, m_cursor - content_start);
      advance();
      return make_token(TokenType::StringLiteral, text, open_loc);
    }
    advance();
  }

  const std::string_view span = m_source.substr(open_index, m_cursor - open_index);
  report(open_loc, span, kUnterminatedString);
  return make_token(TokenType::Invalid, span, open_loc);
}

bool Lexer::skip_template_body() {
  while (!at_line_end()) {
    if (peek() == '\\') {
      advance();
      if (!at_line_end()) {
        advance();
      }
      continue;
    }
    if (peek() == '`') {
      advance();
      return true;
    }
    if (peek() == '$' && peek_next() == '{') {
      advance();
      advance();
      if (!skip_balanced_braces()) {
        return false;
      }
      advance();
      continue;
    }
    advance();
  }
  return false;
}

bool Lexer::skip_balanced_braces() {
  int depth = 1;
  while (!at_line_end()) {
    if (peek() == '\\') {
      advance();
      if (!at_line_end()) {
        advance();
      }
      continue;
    }
    if (peek() == '"' || peek() == '\'') {
      const char quote = advance();
      while (!at_line_end() && peek() != quote) {
        if (peek() == '\\') {
          advance();
          if (!at_line_end()) {
            advance();
          }
          continue;
        }
        advance();
      }
      if (!is_at_end() && peek() == quote) {
        advance();
      }
      continue;
    }
    if (peek() == '`') {
      advance();
      if (!skip_template_body()) {
        return false;
      }
      continue;
    }
    if (peek() == '{') {
      ++depth;
      advance();
      continue;
    }
    if (peek() == '}') {
      --depth;
      if (depth == 0) {
        return true;
      }
      advance();
      continue;
    }
    advance();
  }
  return false;
}

void Lexer::read_template(const SourceLocation open_loc, const std::size_t open_index, std::vector<Token>& tokens) {
  std::vector<Token> produced;
  std::vector<Diagnostic> notes;
  const CursorState origin = save_state();
  bool saw_hole = false;
  bool closed = false;
  while (!at_line_end()) {
    const SourceLocation literal_loc{m_line, m_column};
    const std::size_t literal_start = m_cursor;
    bool hole = false;
    while (!at_line_end()) {
      if (peek() == '\\') {
        advance();
        if (!at_line_end()) {
          advance();
        }
        continue;
      }
      if (peek() == '`') {
        const TokenType type = saw_hole ? TokenType::TemplateTail : TokenType::TemplateString;
        produced.push_back(make_token(type, m_source.substr(literal_start, m_cursor - literal_start), literal_loc));
        advance();
        closed = true;
        break;
      }
      if (peek() == '$' && peek_next() == '{') {
        produced.push_back(make_token(saw_hole ? TokenType::TemplateMiddle : TokenType::TemplateHead,
                                      m_source.substr(literal_start, m_cursor - literal_start), literal_loc));
        advance();
        advance();
        const std::uint32_t hole_line = m_line;
        const std::uint32_t hole_column = m_column;
        const std::size_t hole_start = m_cursor;
        if (!skip_balanced_braces()) {
          hole = false;
          closed = false;
          break;
        }
        const std::size_t hole_end = m_cursor;
        Lexer nested(m_source.substr(hole_start, hole_end - hole_start));
        LexResult inner = nested.tokenize();
        for (Token& token : inner.tokens) {
          if (token.type == TokenType::Eof) {
            break;
          }
          token.location.line += hole_line - 1;
          token.location.column += hole_column - 1;
          produced.push_back(token);
        }
        for (Diagnostic& diagnostic : inner.diagnostics) {
          diagnostic.location.line += hole_line - 1;
          diagnostic.location.column += hole_column - 1;
          notes.push_back(diagnostic);
        }
        advance();
        saw_hole = true;
        hole = true;
        break;
      }
      advance();
    }
    if (closed) {
      break;
    }
    if (!hole) {
      break;
    }
  }
  if (!closed) {
    restore_state(origin);
    tokens.push_back(read_template_string(open_loc, open_index));
    return;
  }
  m_diagnostics.insert(m_diagnostics.end(), notes.begin(), notes.end());
  tokens.insert(tokens.end(), produced.begin(), produced.end());
}

Token Lexer::read_template_string(const SourceLocation open_loc, const std::size_t open_index) {
  const std::size_t content_start = m_cursor;

  while (!is_at_end()) {
    if (peek() == '\n' || peek() == '\r') {
      break;
    }
    if (peek() == '\\') {
      advance();
      if (!at_line_end()) {
        advance();
      }
      continue;
    }
    if (peek() == '`') {
      const std::string_view text = m_source.substr(content_start, m_cursor - content_start);
      advance();
      return make_token(TokenType::TemplateString, text, open_loc);
    }
    advance();
  }

  const std::string_view span = m_source.substr(open_index, m_cursor - open_index);
  report(open_loc, span, kUnterminatedTemplate);
  return make_token(TokenType::Invalid, span, open_loc);
}

std::string_view token_type_to_string(TokenType type) {
  switch (type) {
    case TokenType::Eof: return "Eof";
    case TokenType::Invalid: return "Invalid";
    case TokenType::Identifier: return "Identifier";
    case TokenType::IntLiteral: return "IntLiteral";
    case TokenType::FloatLiteral: return "FloatLiteral";
    case TokenType::StringLiteral: return "StringLiteral";
    case TokenType::TemplateString: return "TemplateString";
    case TokenType::TemplateHead: return "TemplateHead";
    case TokenType::TemplateMiddle: return "TemplateMiddle";
    case TokenType::TemplateTail: return "TemplateTail";
    case TokenType::DocComment: return "DocComment";
    case TokenType::KwAnd: return "and";
    case TokenType::KwOr: return "or";
    case TokenType::KwNot: return "not";
    case TokenType::KwVoid: return "void";
    case TokenType::KwInt: return "int";
    case TokenType::KwFloat: return "float";
    case TokenType::KwDouble: return "double";
    case TokenType::KwString: return "string";
    case TokenType::KwBool: return "bool";
    case TokenType::KwAuto: return "auto";
    case TokenType::KwFunc: return "func";
    case TokenType::KwNull: return "null";
    case TokenType::KwTrue: return "true";
    case TokenType::KwType: return "type";
    case TokenType::KwFalse: return "false";
    case TokenType::KwStruct: return "struct";
    case TokenType::KwConst: return "const";
    case TokenType::KwStatic: return "static";
    case TokenType::KwConstexpr: return "constexpr";
    case TokenType::KwNew: return "new";
    case TokenType::KwLink: return "link";
    case TokenType::KwAs: return "as";
    case TokenType::KwFrom: return "from";
    case TokenType::KwLet: return "let";
    case TokenType::KwJoin: return "join";
    case TokenType::KwThread: return "thread";
    case TokenType::KwAtomic: return "atomic";
    case TokenType::KwDecltype: return "decltype";
    case TokenType::KwMove: return "move";
    case TokenType::KwOperator: return "operator";
    case TokenType::KwVariant: return "variant";
    case TokenType::KwMut: return "mut";
    case TokenType::KwImport: return "import";
    case TokenType::KwUsing: return "using";
    case TokenType::KwEnum: return "enum";
    case TokenType::KwExtern: return "extern";
    case TokenType::KwAbstract: return "abstract";
    case TokenType::KwOverride: return "override";
    case TokenType::KwPrivate: return "private";
    case TokenType::KwIf: return "if";
    case TokenType::KwElse: return "else";
    case TokenType::KwWhile: return "while";
    case TokenType::KwFor: return "for";
    case TokenType::KwIn: return "in";
    case TokenType::KwReturn: return "return";
    case TokenType::KwBreak: return "break";
    case TokenType::KwContinue: return "continue";
    case TokenType::KwGuard: return "guard";
    case TokenType::KwMatch: return "match";
    case TokenType::KwSwitch: return "switch";
    case TokenType::KwCase: return "case";
    case TokenType::KwDefault: return "default";
    case TokenType::KwAsync: return "async";
    case TokenType::KwAwait: return "await";
    case TokenType::KwSpawn: return "spawn";
    case TokenType::KwParallel: return "parallel";
    case TokenType::KwTask: return "task";
    case TokenType::KwObservable: return "observable";
    case TokenType::KwSignal: return "signal";
    case TokenType::KwBuffer: return "buffer";
    case TokenType::KwVector2: return "Vector2";
    case TokenType::KwVector3: return "Vector3";
    case TokenType::KwVector4: return "Vector4";
    case TokenType::KwPost: return "post";
    case TokenType::KwWarn: return "warn";
    case TokenType::KwReport: return "report";
    case TokenType::KwCout: return "cout";
    case TokenType::KwEndl: return "endl";
    case TokenType::KwPcall: return "pcall";
    case TokenType::KwPublic: return "public";
    case TokenType::KwFinal: return "final";
    case TokenType::KwNamespace: return "namespace";
    case TokenType::KwWhere: return "where";
    case TokenType::KwTry: return "try";
    case TokenType::KwCatch: return "catch";
    case TokenType::KwShl: return "shl";
    case TokenType::KwShr: return "shr";
    case TokenType::OpenParen: return "(";
    case TokenType::CloseParen: return ")";
    case TokenType::OpenBrace: return "{";
    case TokenType::CloseBrace: return "}";
    case TokenType::OpenBracket: return "[";
    case TokenType::CloseBracket: return "]";
    case TokenType::Semicolon: return ";";
    case TokenType::Comma: return ",";
    case TokenType::At: return "@";
    case TokenType::Hash: return "#";
    case TokenType::Plus: return "+";
    case TokenType::PlusPlus: return "++";
    case TokenType::Minus: return "-";
    case TokenType::MinusMinus: return "--";
    case TokenType::Star: return "*";
    case TokenType::Slash: return "/";
    case TokenType::Percent: return "%";
    case TokenType::PlusEq: return "+=";
    case TokenType::MinusEq: return "-=";
    case TokenType::StarEq: return "*=";
    case TokenType::SlashEq: return "/=";
    case TokenType::PercentEq: return "%=";
    case TokenType::Equal: return "=";
    case TokenType::EqualEqual: return "==";
    case TokenType::NotEqual: return "!=";
    case TokenType::Bang: return "!";
    case TokenType::Question: return "?";
    case TokenType::Less: return "<";
    case TokenType::LessEqual: return "<=";
    case TokenType::Greater: return ">";
    case TokenType::GreaterEqual: return ">=";
    case TokenType::Dot: return ".";
    case TokenType::DotDot: return "..";
    case TokenType::Ellipsis: return "...";
    case TokenType::Concat: return ".:";
    case TokenType::Arrow: return "->";
    case TokenType::FatArrow: return "=>";
    case TokenType::Colon: return ":";
    case TokenType::Pipe: return "|";
    case TokenType::Ampersand: return "&";
    case TokenType::Caret: return "^";
    case TokenType::BitNot: return "~";
    case TokenType::Mode: return "mode";
    case TokenType::Scope: return "::";
    case TokenType::SignalConnect: return "~>";
  }
  return "Unknown";
}

}  // namespace clpp
