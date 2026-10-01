#include "core/parser/parser.hpp"

#include "clpp/core/lexer/lexer.hpp"

#include <cctype>
#include <charconv>
#include <locale>
#include <sstream>
#include <cmath>
#include <string>
#include <system_error>
#include <utility>

namespace clpp::parser {

namespace {

const Token kSyntheticEof{TokenType::Eof, {}, {}};

[[nodiscard]] std::optional<std::string> decode_escapes(const std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index) {
    if (raw[index] != '\\') {
      out.push_back(raw[index]);
      continue;
    }
    if (index + 1 >= raw.size()) {
      return std::nullopt;
    }
    const char next = raw[++index];
    switch (next) {
      case 'n':
        out.push_back('\n');
        break;
      case 't':
        out.push_back('\t');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case '0':
        out.push_back('\0');
        break;
      case '\\':
      case '\'':
      case '"':
      case '`':
      case '$':
        out.push_back(next);
        break;
      default:
        return std::nullopt;
    }
  }
  return out;
}

[[nodiscard]] bool is_type_name(const TokenType type) {
  return type == TokenType::KwVector2 || type == TokenType::KwVector3 || type == TokenType::KwVector4 ||
         type == TokenType::KwBuffer || type == TokenType::KwInt || type == TokenType::KwFloat ||
         type == TokenType::KwDouble || type == TokenType::KwString || type == TokenType::KwBool ||
         type == TokenType::KwTask;
}

[[nodiscard]] bool is_callable_name(const TokenType type) {
  return type == TokenType::Identifier || is_type_name(type);
}

// "@clpp.axiom" -> "Axiom", "./ui/hud_bar.clp" -> "Hud_bar": the last name, first letter upper case.
[[nodiscard]] std::string default_alias(const std::string& path) {
  std::size_t start = path.find_last_of("./\\");
  std::string name = path;
  if (name.size() > 4 && name.compare(name.size() - 4, 4, ".clp") == 0) {
    name.resize(name.size() - 4);
    start = name.find_last_of("./\\");
  }
  name = start == std::string::npos ? name : name.substr(start + 1);
  for (const char character : name) {
    if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_') {
      return {};
    }
  }
  if (!name.empty() && std::isdigit(static_cast<unsigned char>(name[0])) == 0) {
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return name;
  }
  return {};
}

[[nodiscard]] Expr make_binary(const Expr::Kind kind, Expr left, Expr right) {
  Expr combined;
  combined.kind = kind;
  // Diagnostics on `a + b` point at (and underline) the whole expression, not at 1:1.
  combined.location = left.location;
  combined.span = left.span;
  if (!left.span.empty() && !right.span.empty() && right.span.data() >= left.span.data()) {
    combined.span = std::string_view(left.span.data(),
                                     static_cast<std::size_t>(right.span.data() + right.span.size() - left.span.data()));
  }
  combined.left = std::make_unique<Expr>(std::move(left));
  combined.right = std::make_unique<Expr>(std::move(right));
  return combined;
}

[[nodiscard]] bool name_taken(const Program& program, const std::string_view name) {
  for (const StructDecl& decl : program.structs) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const EnumDecl& decl : program.enums) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const VariantDecl& decl : program.variants) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const Function& function : program.functions) {
    if (function.name == name) {
      return true;
    }
  }
  for (const LinkDecl& link : program.links) {
    if (link.alias == name) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool function_slot_taken(const Program& program, const std::string_view name, const std::size_t arity) {
  for (const StructDecl& decl : program.structs) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const EnumDecl& decl : program.enums) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const VariantDecl& decl : program.variants) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const LinkDecl& link : program.links) {
    if (link.alias == name) {
      return true;
    }
  }
  for (const Function& function : program.functions) {
    if (function.name == name && function.params.size() == arity) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::size_t find_interpolation_end(const std::string_view text, const std::size_t open_brace) {
  int depth = 1;
  for (std::size_t index = open_brace + 1; index < text.size(); ++index) {
    const char current = text[index];
    if (current == '\\' && index + 1 < text.size()) {
      ++index;
      continue;
    }
    if (current == '"' || current == '\'') {
      const char quote = current;
      ++index;
      while (index < text.size() && text[index] != quote) {
        if (text[index] == '\\' && index + 1 < text.size()) {
          index += 2;
        } else {
          ++index;
        }
      }
      continue;
    }
    if (current == '{') {
      ++depth;
    } else if (current == '}') {
      --depth;
      if (depth == 0) {
        return index;
      }
    }
  }
  return std::string_view::npos;
}

[[nodiscard]] bool parse_number_lexeme(const std::string_view lexeme, double& number) {
  std::string body;
  body.reserve(lexeme.size());
  for (const char c : lexeme) {
    if (c != '_') {
      body.push_back(c);
    }
  }
  const char* const first = body.data();
  const char* const last = body.data() + body.size();
  if (body.size() >= 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X' || body[1] == 'b' || body[1] == 'B')) {
    const int base = (body[1] == 'b' || body[1] == 'B') ? 2 : 16;
    long long integer = 0;
    const std::from_chars_result parsed = std::from_chars(first + 2, last, integer, base);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
      return false;
    }
    number = static_cast<double>(integer);
    return true;
  }
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L && !defined(_LIBCPP_VERSION)
  const std::from_chars_result parsed = std::from_chars(first, last, number);
  return parsed.ec == std::errc{} && parsed.ptr == last;
#else
  // libc++ (macOS, llvm-mingw) has no floating-point from_chars yet. The lexer already validated
  // the literal, so strtod in the "C" locale reads it the same way.
  std::istringstream stream(body);
  stream.imbue(std::locale::classic());
  stream >> number;
  return !stream.fail() && stream.peek() == std::char_traits<char>::eof();
#endif
}

}  // namespace

Expr::Expr() = default;
Expr::Expr(Expr&&) noexcept = default;
Expr& Expr::operator=(Expr&&) noexcept = default;
Expr::~Expr() = default;

Parser::Parser(const std::span<const Token> tokens, const std::span<const Diagnostic> lexer_diagnostics)
    : m_tokens(tokens), m_lexer_diagnostics(lexer_diagnostics) {}

ParseResult Parser::parse() {
  m_diagnostics.assign(m_lexer_diagnostics.begin(), m_lexer_diagnostics.end());
  Program program;
  m_program = &program;

  bool mode_set = false;
  while (!is_at_end()) {
    if (check(TokenType::Mode)) {
      if (mode_set) {
        error("already declared");
      } else {
        mode_set = true;
        if (peek().lexeme == "strict") {
          program.mode = CheckMode::Strict;
        } else if (peek().lexeme == "nocheck") {
          program.mode = CheckMode::Nocheck;
        } else {
          program.mode = CheckMode::Nonstrict;
        }
      }
      advance();
      continue;
    }
    if (check(TokenType::DocComment) || check(TokenType::Invalid)) {
      advance();
      continue;
    }
    if (check(TokenType::OpenBracket) && peek_at(1).type == TokenType::OpenBracket) {
      advance();
      advance();
      while (!check(TokenType::CloseBracket) && !is_at_end()) {
        advance();
      }
      if (check(TokenType::CloseBracket)) {
        advance();
      }
      if (check(TokenType::CloseBracket)) {
        advance();
      }
      continue;
    }
    if (check(TokenType::KwNamespace)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        continue;
      }
      const std::string prefix = std::string(peek().lexeme) + ".";
      advance();
      if (!consume(TokenType::OpenBrace, "expected '{'")) {
        synchronize();
        continue;
      }
      while (!check(TokenType::CloseBrace) && !is_at_end()) {
        if (check(TokenType::KwFunc)) {
          if (std::optional<Function> function = parse_function()) {
            function->name = prefix + function->name;
            program.functions.push_back(std::move(*function));
          }
        } else {
          advance();
        }
      }
      if (check(TokenType::CloseBrace)) {
        advance();
      }
      continue;
    }
    if (check(TokenType::KwTry)) {
      if (std::optional<Stmt> stmt = parse_try()) {
        program.statements.push_back(std::move(*stmt));
      }
      continue;
    }
    if (check(TokenType::KwFinal) && peek_at(1).type == TokenType::KwStruct) {
      advance();
      if (std::optional<StructDecl> decl = parse_struct()) {
        decl->is_final = true;
        if (!name_taken(program, decl->name)) {
          for (Function& method : decl->methods) {
            program.functions.push_back(std::move(method));
          }
          program.structs.push_back(std::move(*decl));
        }
      }
      continue;
    }
    if (check(TokenType::KwStatic) &&
        (peek_at(1).type == TokenType::KwFunc || peek_at(1).type == TokenType::KwVoid ||
         peek_at(1).type == TokenType::KwAsync || peek_at(1).type == TokenType::KwStruct ||
         peek_at(1).type == TokenType::KwEnum)) {
      advance();
    }
    if (check(TokenType::KwLink) || check(TokenType::KwImport) || check(TokenType::KwFrom)) {
      (void)parse_link(program);
      continue;
    }
    if (check(TokenType::KwVoid)) {
      if (std::optional<Function> function = parse_void_function()) {
        if (function_slot_taken(program, function->name, function->params.size())) {
          m_diagnostics.push_back(
              Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (check(TokenType::KwAsync)) {
      advance();
      if (!check(TokenType::KwFunc)) {
        error("expected func");
        synchronize();
        continue;
      }
      if (std::optional<Function> function = parse_function()) {
        function->is_async = true;
        if (function_slot_taken(program, function->name, function->params.size())) {
          m_diagnostics.push_back(
              Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (check(TokenType::KwExtern)) {
      if (std::optional<Function> function = parse_extern()) {
        if (function_slot_taken(program, function->name, function->params.size())) {
          m_diagnostics.push_back(Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (check(TokenType::KwFunc)) {
      if (std::optional<Function> function = parse_function()) {
        if (function_slot_taken(program, function->name, function->params.size())) {
          m_diagnostics.push_back(
              Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (check(TokenType::KwAbstract) && peek_at(1).type == TokenType::KwStruct) {
      advance();
      if (std::optional<StructDecl> decl = parse_struct()) {
        decl->is_abstract = true;
        if (name_taken(program, decl->name)) {
          m_diagnostics.push_back(Diagnostic{decl->name_location, decl->name_span, "already declared"});
        } else {
          for (Function& method : decl->methods) {
            if (name_taken(program, method.name)) {
              m_diagnostics.push_back(Diagnostic{method.name_location, method.name_span, "already declared"});
            } else {
              program.functions.push_back(std::move(method));
            }
          }
          program.structs.push_back(std::move(*decl));
        }
      }
      continue;
    }
    if (check(TokenType::KwStruct)) {
      if (std::optional<StructDecl> decl = parse_struct()) {
        if (name_taken(program, decl->name)) {
          m_diagnostics.push_back(Diagnostic{decl->name_location, decl->name_span, "already declared"});
        } else {
          for (Function& method : decl->methods) {
            if (name_taken(program, method.name)) {
              m_diagnostics.push_back(Diagnostic{method.name_location, method.name_span, "already declared"});
            } else {
              program.functions.push_back(std::move(method));
            }
          }
          program.structs.push_back(std::move(*decl));
        }
      }
      continue;
    }
    if (check(TokenType::KwVariant)) {
      if (std::optional<VariantDecl> decl = parse_variant()) {
        if (name_taken(program, decl->name)) {
          m_diagnostics.push_back(Diagnostic{decl->name_location, decl->name_span, "already declared"});
        } else {
          program.variants.push_back(std::move(*decl));
        }
      }
      continue;
    }
    if (check(TokenType::KwType)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        continue;
      }
      const Token name = peek();
      advance();
      if (!consume(TokenType::Equal, "expected '='")) {
        synchronize();
        continue;
      }
      const std::string body = parse_type_annotation();
      if (!consume(TokenType::Semicolon, "expected ';'")) {
        synchronize();
        continue;
      }
      if (body.empty() || name_taken(program, name.lexeme)) {
        if (!body.empty()) {
          m_diagnostics.push_back(Diagnostic{name.location, name.lexeme, "already declared"});
        }
        continue;
      }
      program.aliases.push_back(TypeAlias{std::string(name.lexeme), body});
      continue;
    }
    if (check(TokenType::KwEnum)) {
      if (std::optional<EnumDecl> decl = parse_enum()) {
        if (name_taken(program, decl->name)) {
          m_diagnostics.push_back(Diagnostic{decl->name_location, decl->name_span, "already declared"});
        } else {
          program.enums.push_back(std::move(*decl));
        }
      }
      continue;
    }
    const std::size_t before = m_current;
    if (std::optional<Stmt> stmt = parse_statement()) {
      program.statements.push_back(std::move(*stmt));
    } else if (m_current == before) {
      advance();
    }
  }

  ParseResult result;
  result.program = std::move(program);
  result.diagnostics = std::move(m_diagnostics);
  return result;
}

bool Parser::is_at_end() const {
  return m_current >= m_tokens.size() || m_tokens[m_current].type == TokenType::Eof;
}

bool Parser::check(const TokenType type) const {
  return peek().type == type;
}

const Token& Parser::peek() const {
  return peek_at(0);
}

const Token& Parser::peek_at(const std::size_t offset) const {
  if (m_current + offset >= m_tokens.size()) {
    return kSyntheticEof;
  }
  return m_tokens[m_current + offset];
}

void Parser::advance() {
  if (!is_at_end()) {
    ++m_current;
  }
}

bool Parser::consume(const TokenType type, const std::string_view message) {
  if (check(type)) {
    advance();
    return true;
  }
  error(message);
  return false;
}

void Parser::error(const std::string_view message) {
  const Token& token = peek();
  m_diagnostics.push_back(Diagnostic{token.location, token.lexeme, std::string(message)});
}

void Parser::synchronize() {
  while (!is_at_end()) {
    if (peek().type == TokenType::Semicolon) {
      advance();
      return;
    }
    if (m_block_depth > 0 && peek().type == TokenType::CloseBrace) {
      return;
    }
    advance();
  }
}

std::optional<Stmt> Parser::parse_statement() {
  if (check(TokenType::KwStatic)) {
    advance();
  }
  if (check(TokenType::KwPost) || check(TokenType::KwCout) || check(TokenType::KwWarn) || check(TokenType::KwReport)) {
    return parse_post();
  }
  if (check(TokenType::KwLet)) {
    return parse_let();
  }
  if (check(TokenType::KwTry)) {
    return parse_try();
  }
  if (check(TokenType::Identifier) &&
      (peek_at(1).type == TokenType::PlusPlus || peek_at(1).type == TokenType::MinusMinus)) {
    std::optional<Expr> expr = parse_expression();
    if (!expr || !consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return std::nullopt;
    }
    Stmt stmt;
    stmt.kind = Stmt::Kind::Expr;
    stmt.expr = std::move(*expr);
    return stmt;
  }
  if (check(TokenType::KwAtomic)) {
    advance();
    std::optional<Stmt> stmt = parse_initialized_name({});
    if (!stmt) {
      return std::nullopt;
    }
    stmt->atomic_cell = true;
    stmt->immutable = true;
    return stmt;
  }
  if (check(TokenType::KwDecltype)) {
    advance();
    if (!consume(TokenType::OpenParen, "expected '('")) {
      synchronize();
      return std::nullopt;
    }
    std::optional<Expr> type_expr = parse_expression();
    if (!type_expr) {
      synchronize();
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      synchronize();
      return std::nullopt;
    }
    std::optional<Stmt> stmt = parse_initialized_name({});
    if (!stmt) {
      return std::nullopt;
    }
    stmt->has_decltype = true;
    stmt->type_expr = std::move(*type_expr);
    stmt->immutable = true;
    return stmt;
  }
  if (check(TokenType::KwAuto)) {
    return parse_auto();
  }
  if (check(TokenType::KwObservable)) {
    return parse_observable();
  }
  if (check(TokenType::KwConst)) {
    return parse_const();
  }
  if (check(TokenType::KwConstexpr)) {
    return parse_constexpr();
  }
  if (check(TokenType::KwSignal)) {
    return parse_signal();
  }
  if (check(TokenType::KwUsing)) {
    return parse_using();
  }
  if (check(TokenType::Identifier) && peek_at(1).type == TokenType::SignalConnect) {
    return parse_connect();
  }
  if (check(TokenType::KwBreak) || check(TokenType::KwContinue)) {
    return parse_break();
  }
  if (check(TokenType::KwFor)) {
    return parse_for();
  }
  if (check(TokenType::KwSwitch)) {
    return parse_switch();
  }
  if (check(TokenType::Identifier) &&
      (peek_at(1).type == TokenType::PlusEq || peek_at(1).type == TokenType::MinusEq ||
       peek_at(1).type == TokenType::StarEq || peek_at(1).type == TokenType::SlashEq ||
       peek_at(1).type == TokenType::PercentEq)) {
    return parse_compound_assign(true);
  }
  if (check(TokenType::Identifier) && peek_at(1).type == TokenType::Equal) {
    return parse_assign();
  }
  if (at_place_assign()) {
    return parse_place_assign();
  }
  if (check(TokenType::KwIf)) {
    return parse_if();
  }
  if (check(TokenType::KwWhile)) {
    return parse_while();
  }
  if (check(TokenType::KwMatch)) {
    return parse_match();
  }
  if (check(TokenType::KwReturn)) {
    return parse_return();
  }
  if (check(TokenType::At)) {
    std::optional<Expr> field = parse_self_field();
    if (!field) {
      synchronize();
      return std::nullopt;
    }
    if (!consume(TokenType::Equal, "expected '='")) {
      synchronize();
      return std::nullopt;
    }
    std::optional<Expr> value = parse_expression();
    if (!value) {
      synchronize();
      return std::nullopt;
    }
    if (!consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return std::nullopt;
    }
    Stmt stmt;
    stmt.kind = Stmt::Kind::SelfAssign;
    stmt.name = field->value;
    stmt.name_span = field->span;
    stmt.name_location = field->location;
    stmt.expr = std::move(*value);
    return stmt;
  }
  bool generic_decl = false;
  if ((is_type_name(peek().type) || check(TokenType::Identifier)) && peek_at(1).type == TokenType::Less) {
    int depth = 0;
    for (std::size_t index = 1;; ++index) {
      const TokenType type = peek_at(index).type;
      if (type == TokenType::Eof) {
        break;
      }
      if (type == TokenType::Less) {
        ++depth;
      } else if (type == TokenType::Greater) {
        --depth;
        if (depth == 0) {
          generic_decl = peek_at(index + 1).type == TokenType::Identifier && peek_at(index + 2).type == TokenType::Equal;
          break;
        }
      }
    }
  }
  if ((is_type_name(peek().type) || check(TokenType::Identifier)) &&
      (generic_decl || (peek_at(1).type == TokenType::Identifier && peek_at(2).type == TokenType::Equal) ||
       (check(TokenType::Identifier) && peek_at(1).type == TokenType::Dot && peek_at(2).type == TokenType::Identifier &&
        peek_at(3).type == TokenType::Identifier && peek_at(4).type == TokenType::Equal))) {
    const std::string declared_type = parse_type_annotation();
    if (declared_type.empty()) {
      return std::nullopt;
    }
    return parse_initialized_name(declared_type);
  }
  if (check(TokenType::KwJoin) ||
      (is_callable_name(peek().type) &&
       (peek_at(1).type == TokenType::OpenParen || peek_at(1).type == TokenType::Scope ||
        (peek_at(1).type == TokenType::Dot && peek_at(2).type == TokenType::Identifier &&
         peek_at(3).type == TokenType::OpenParen)))) {
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      synchronize();
      return std::nullopt;
    }
    if (!consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return std::nullopt;
    }
    Stmt stmt;
    stmt.kind = Stmt::Kind::Expr;
    stmt.expr = std::move(*expr);
    return stmt;
  }
  if (is_callable_name(peek().type) && peek_at(1).type == TokenType::Dot) {
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      synchronize();
      return std::nullopt;
    }
    if (check(TokenType::Semicolon)) {
      advance();
    } else if (!expr->missing) {
      error("expected ';'");
    }
    Stmt stmt;
    stmt.kind = Stmt::Kind::Expr;
    stmt.expr = std::move(*expr);
    return stmt;
  }
  error("not in this subset");
  synchronize();
  return std::nullopt;
}

std::optional<Stmt> Parser::parse_post() {
  const std::uint8_t channel = check(TokenType::KwWarn) ? 1 : check(TokenType::KwReport) ? 2 : 0;
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }

  std::optional<Expr> expr = parse_expression();
  if (!expr) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::Post;
  stmt.channel = channel;
  stmt.expr = std::move(*expr);
  return stmt;
}

std::optional<Stmt> Parser::parse_let() {
  advance();
  const bool mut = check(TokenType::KwMut);
  if (mut) {
    advance();
  }
  if (check(TokenType::OpenParen)) {
    advance();
    Stmt stmt;
    stmt.kind = Stmt::Kind::Let;
    stmt.immutable = !mut;
    while (check(TokenType::Identifier)) {
      stmt.unpack.emplace_back(peek().lexeme);
      if (stmt.name.empty()) {
        stmt.name = std::string(peek().lexeme);
        stmt.name_span = peek().lexeme;
        stmt.name_location = peek().location;
      }
      advance();
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
    if (stmt.unpack.size() < 2 || !consume(TokenType::CloseParen, "expected ')'") ||
        !consume(TokenType::Equal, "expected '='")) {
      error("expected name");
      synchronize();
      return std::nullopt;
    }
    std::optional<Expr> expr = parse_expression();
    if (!expr || !consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return std::nullopt;
    }
    stmt.expr = std::move(*expr);
    return stmt;
  }
  std::optional<Stmt> stmt = parse_initialized_name({});
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = !mut;
  return stmt;
}

std::optional<Stmt> Parser::parse_auto() {
  advance();
  std::optional<Stmt> stmt = parse_initialized_name({});
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = true;
  return stmt;
}

std::optional<Stmt> Parser::parse_observable() {
  advance();
  std::string type_name;
  if (is_type_name(peek().type) && peek_at(1).type == TokenType::Identifier) {
    type_name = std::string(peek().lexeme);
    advance();
  }
  std::optional<Stmt> stmt = parse_initialized_name(std::move(type_name));
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = false;
  stmt->observable_cell = true;
  return stmt;
}

std::optional<double> fold_number(const Expr& expr, bool& division_by_zero) {
  if (expr.kind == Expr::Kind::Number) {
    return expr.number;
  }
  if (expr.left == nullptr || expr.right == nullptr) {
    return std::nullopt;
  }
  if (expr.kind != Expr::Kind::Add && expr.kind != Expr::Kind::Sub && expr.kind != Expr::Kind::Mul &&
      expr.kind != Expr::Kind::Div && expr.kind != Expr::Kind::Mod) {
    return std::nullopt;
  }
  const std::optional<double> left = fold_number(*expr.left, division_by_zero);
  const std::optional<double> right = fold_number(*expr.right, division_by_zero);
  if (!left || !right || division_by_zero) {
    return std::nullopt;
  }
  if ((expr.kind == Expr::Kind::Div || expr.kind == Expr::Kind::Mod) && *right == 0) {
    division_by_zero = true;
    return std::nullopt;
  }
  if (expr.kind == Expr::Kind::Add) {
    return *left + *right;
  }
  if (expr.kind == Expr::Kind::Sub) {
    return *left - *right;
  }
  if (expr.kind == Expr::Kind::Mul) {
    return *left * *right;
  }
  if (expr.kind == Expr::Kind::Div) {
    return *left / *right;
  }
  return std::fmod(*left, *right);
}

std::optional<Stmt> Parser::parse_constexpr() {
  advance();
  std::optional<Stmt> stmt = parse_initialized_name({});
  if (!stmt) {
    return std::nullopt;
  }
  bool division_by_zero = false;
  const std::optional<double> folded = fold_number(stmt->expr, division_by_zero);
  if (division_by_zero) {
    error("division by zero");
    return std::nullopt;
  }
  if (!folded) {
    error("not a constant");
    return std::nullopt;
  }
  stmt->expr = Expr{};
  stmt->expr.kind = Expr::Kind::Number;
  stmt->expr.number = *folded;
  stmt->immutable = true;
  return stmt;
}

std::optional<Stmt> Parser::parse_signal() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Signal;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  return stmt;
}

std::optional<Stmt> Parser::parse_connect() {
  const Token name = peek();
  advance();
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  std::string callee(peek().lexeme);
  advance();
  while (check(TokenType::Dot) && peek_at(1).type == TokenType::Identifier) {  // damaged ~> Hud.onDamage
    advance();
    callee += ".";
    callee += std::string(peek().lexeme);
    advance();
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Connect;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.declared_type = std::move(callee);
  return stmt;
}

std::optional<Stmt> Parser::parse_using() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token alias = peek();
  advance();
  if (!consume(TokenType::Dot, "expected '.'")) {
    synchronize();
    return std::nullopt;
  }
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token member = peek();
  advance();
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Using;
  stmt.name = std::string(member.lexeme);
  stmt.name_span = member.lexeme;
  stmt.name_location = member.location;
  stmt.declared_type = std::string(alias.lexeme) + "." + stmt.name;
  return stmt;
}

std::optional<Stmt> Parser::parse_const() {
  advance();
  std::optional<Stmt> stmt = parse_initialized_name({});
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = true;
  return stmt;
}

std::optional<Stmt> Parser::parse_initialized_name(std::string declared_type) {
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  if (declared_type.empty() && check(TokenType::Colon)) {
    advance();
    declared_type = parse_type_annotation();
  }
  if (!consume(TokenType::Equal, "expected '='")) {
    synchronize();
    return std::nullopt;
  }
  const auto begins_statement = [&]() {
    if (is_at_end()) {
      return true;
    }
    if (peek().location.line <= name.location.line) {
      return false;
    }
    const TokenType type = peek().type;
    return is_type_name(type) || type == TokenType::KwLet || type == TokenType::KwFunc || type == TokenType::KwPost ||
           type == TokenType::KwIf || type == TokenType::KwWhile || type == TokenType::KwFor ||
           type == TokenType::KwReturn || type == TokenType::KwStruct || type == TokenType::KwEnum;
  };
  std::optional<Expr> expr;
  if (begins_statement()) {
    error("expected expression");
    Expr missing;
    missing.kind = Expr::Kind::Number;
    missing.missing = true;
    missing.location = peek().location;
    missing.span = peek().lexeme;
    expr = std::move(missing);
  } else {
    expr = parse_expression();
    if (!expr) {
      Expr missing;
      missing.kind = Expr::Kind::Number;
      missing.missing = true;
      missing.location = peek().location;
      missing.span = peek().lexeme;
      expr = std::move(missing);
      if (check(TokenType::Semicolon)) {
        advance();
      }
    }
  }
  if (expr->missing) {
    if (check(TokenType::Semicolon)) {
      advance();
    }
  } else if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::Let;
  stmt.declared_type = std::move(declared_type);
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.expr = std::move(*expr);
  return stmt;
}

std::optional<Stmt> Parser::parse_compound_assign(const bool require_semicolon) {
  const Token name = peek();
  advance();
  const Token op = peek();
  Expr::Kind kind = Expr::Kind::Add;
  if (op.type == TokenType::MinusEq) {
    kind = Expr::Kind::Sub;
  } else if (op.type == TokenType::StarEq) {
    kind = Expr::Kind::Mul;
  } else if (op.type == TokenType::SlashEq) {
    kind = Expr::Kind::Div;
  } else if (op.type == TokenType::PercentEq) {
    kind = Expr::Kind::Mod;
  }
  advance();
  std::optional<Expr> rhs = parse_expression();
  if (!rhs) {
    synchronize();
    return std::nullopt;
  }
  if (require_semicolon && !consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Expr lhs;
  lhs.kind = Expr::Kind::Name;
  lhs.value = std::string(name.lexeme);
  lhs.span = name.lexeme;
  lhs.location = name.location;
  Stmt stmt;
  stmt.kind = Stmt::Kind::Assign;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.expr = make_binary(kind, std::move(lhs), std::move(*rhs));
  return stmt;
}

std::optional<Stmt> Parser::parse_update(const bool require_semicolon) {
  if (peek_at(1).type == TokenType::PlusEq || peek_at(1).type == TokenType::MinusEq ||
      peek_at(1).type == TokenType::StarEq || peek_at(1).type == TokenType::SlashEq ||
      peek_at(1).type == TokenType::PercentEq) {
    return parse_compound_assign(require_semicolon);
  }
  const Token name = peek();
  advance();
  if (!consume(TokenType::Equal, "expected '='")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Expr> expr = parse_expression();
  if (!expr) {
    synchronize();
    return std::nullopt;
  }
  if (require_semicolon && !consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Assign;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.expr = std::move(*expr);
  return stmt;
}

std::optional<Stmt> Parser::parse_assign() {
  const Token name = peek();
  advance();
  advance();
  std::optional<Expr> expr = parse_expression();
  if (!expr) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::Assign;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.expr = std::move(*expr);
  return stmt;
}

// `name(.field | [index])+ (= | += | -= | *= | /= | %=)` — decided by lookahead only, so
// expression statements such as `p.move(1);` or `xs[0];` are untouched.
bool Parser::at_place_assign() const {
  if (!check(TokenType::Identifier)) {
    return false;
  }
  std::size_t at = 1;
  std::size_t steps = 0;
  while (true) {
    const TokenType type = peek_at(at).type;
    if (type == TokenType::Dot && peek_at(at + 1).type == TokenType::Identifier) {
      at += 2;
      ++steps;
      continue;
    }
    if (type == TokenType::OpenBracket) {
      int depth = 0;
      while (true) {
        const TokenType inner = peek_at(at).type;
        if (inner == TokenType::Eof || inner == TokenType::Semicolon || inner == TokenType::OpenBrace) {
          return false;
        }
        if (inner == TokenType::OpenBracket) {
          ++depth;
        } else if (inner == TokenType::CloseBracket && --depth == 0) {
          ++at;
          break;
        }
        ++at;
      }
      ++steps;
      continue;
    }
    break;
  }
  if (steps == 0) {
    return false;
  }
  const TokenType op = peek_at(at).type;
  return op == TokenType::Equal || op == TokenType::PlusEq || op == TokenType::MinusEq || op == TokenType::StarEq ||
         op == TokenType::SlashEq || op == TokenType::PercentEq;
}

std::optional<Expr> Parser::parse_place() {
  const Token root = peek();
  advance();
  Expr place;
  place.kind = Expr::Kind::Name;
  place.value = std::string(root.lexeme);
  place.span = root.lexeme;
  place.location = root.location;
  while (check(TokenType::Dot) || check(TokenType::OpenBracket)) {
    if (check(TokenType::Dot)) {
      advance();
      Expr member;
      member.kind = Expr::Kind::Member;
      member.value = std::string(peek().lexeme);
      member.span = peek().lexeme;
      member.location = peek().location;
      member.left = std::make_unique<Expr>(std::move(place));
      advance();
      place = std::move(member);
      continue;
    }
    const Token open = peek();
    advance();
    std::optional<Expr> index = parse_expression();
    if (!index || !consume(TokenType::CloseBracket, "expected ']'")) {
      return std::nullopt;
    }
    Expr indexed;
    indexed.kind = Expr::Kind::Index;
    indexed.left = std::make_unique<Expr>(std::move(place));
    indexed.right = std::make_unique<Expr>(std::move(*index));
    indexed.location = open.location;
    indexed.span = open.lexeme;
    place = std::move(indexed);
  }
  return place;
}

std::optional<Stmt> Parser::parse_place_assign() {
  const Token root = peek();
  const std::size_t start = m_current;
  std::optional<Expr> place = parse_place();
  if (!place) {
    synchronize();
    return std::nullopt;
  }
  const Token op = peek();
  advance();
  std::optional<Expr> value = parse_expression();
  if (!value) {
    synchronize();
    return std::nullopt;
  }
  if (op.type != TokenType::Equal) {
    // p.hp += 5  ->  p.hp = p.hp + 5 (the place is parsed again to get an independent copy)
    Expr::Kind kind = Expr::Kind::Add;
    if (op.type == TokenType::MinusEq) {
      kind = Expr::Kind::Sub;
    } else if (op.type == TokenType::StarEq) {
      kind = Expr::Kind::Mul;
    } else if (op.type == TokenType::SlashEq) {
      kind = Expr::Kind::Div;
    } else if (op.type == TokenType::PercentEq) {
      kind = Expr::Kind::Mod;
    }
    const std::size_t resume = m_current;
    m_current = start;
    std::optional<Expr> current = parse_place();
    m_current = resume;
    if (!current) {
      synchronize();
      return std::nullopt;
    }
    value = make_binary(kind, std::move(*current), std::move(*value));
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::PlaceAssign;
  stmt.name = std::string(root.lexeme);
  stmt.name_span = root.lexeme;
  stmt.name_location = root.location;
  stmt.place = std::move(*place);
  stmt.expr = std::move(*value);
  return stmt;
}

std::optional<Stmt> Parser::parse_try() {
  advance();
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  if (!consume(TokenType::KwCatch, "expected catch")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Try;
  stmt.then_body = std::move(*body);
  if (check(TokenType::OpenParen)) {
    advance();
    if (check(TokenType::Identifier)) {
      stmt.name = std::string(peek().lexeme);
      stmt.name_span = peek().lexeme;
      stmt.name_location = peek().location;
      advance();
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      synchronize();
      return std::nullopt;
    }
  }
  std::optional<std::vector<Stmt>> handler = parse_block();
  if (!handler) {
    return std::nullopt;
  }
  stmt.else_body = std::move(*handler);
  return stmt;
}

std::optional<Stmt> Parser::parse_if() {
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }
  if (check(TokenType::KwLet)) {
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      synchronize();
      return std::nullopt;
    }
    const Token bound = peek();
    advance();
    if (!consume(TokenType::Equal, "expected '='")) {
      synchronize();
      return std::nullopt;
    }
    std::optional<Expr> condition = parse_expression();
    if (!condition || !consume(TokenType::CloseParen, "expected ')'")) {
      synchronize();
      return std::nullopt;
    }
    std::optional<std::vector<Stmt>> body = parse_block();
    if (!body) {
      return std::nullopt;
    }
    Stmt stmt;
    stmt.kind = Stmt::Kind::If;
    stmt.if_let = true;
    stmt.name = std::string(bound.lexeme);
    stmt.name_span = bound.lexeme;
    stmt.name_location = bound.location;
    stmt.expr = std::move(*condition);
    stmt.then_body = std::move(*body);
    return stmt;
  }
  std::optional<Expr> condition = parse_expression();
  if (!condition) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::If;
  stmt.expr = std::move(*condition);
  stmt.then_body = std::move(*body);
  if (check(TokenType::KwElse)) {
    advance();
    stmt.has_else = true;
    if (check(TokenType::KwIf)) {
      std::optional<Stmt> nested = parse_if();
      if (!nested) {
        return std::nullopt;
      }
      stmt.else_body.push_back(std::move(*nested));
    } else {
      std::optional<std::vector<Stmt>> else_body = parse_block();
      if (!else_body) {
        return std::nullopt;
      }
      stmt.else_body = std::move(*else_body);
    }
  }
  return stmt;
}

std::optional<Stmt> Parser::parse_while() {
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Expr> condition = parse_expression();
  if (!condition) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::While;
  stmt.expr = std::move(*condition);
  stmt.then_body = std::move(*body);
  return stmt;
}

std::optional<Stmt> Parser::parse_break() {
  const bool is_continue = check(TokenType::KwContinue);
  const Token keyword = peek();
  advance();
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Break;  // `continue` shares the statement kind: only codegen tells them apart
  stmt.is_continue = is_continue;
  stmt.name_location = keyword.location;
  stmt.name_span = keyword.lexeme;
  return stmt;
}

std::optional<Stmt> Parser::parse_for_in() {
  advance();
  if (check(TokenType::KwMut)) {
    advance();
  }
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  if (!consume(TokenType::KwIn, "expected 'in'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Expr> limit = parse_expression();
  if (!limit) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::ForIn;
  stmt.name = std::string(name.lexeme);
  stmt.name_span = name.lexeme;
  stmt.name_location = name.location;
  stmt.expr = std::move(*limit);
  stmt.then_body = std::move(*body);
  return stmt;
}

std::optional<Stmt> Parser::parse_for() {
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }
  if (check(TokenType::KwLet)) {
    const std::size_t name_at = peek_at(1).type == TokenType::KwMut ? 2 : 1;
    if (peek_at(name_at).type == TokenType::Identifier && peek_at(name_at + 1).type == TokenType::KwIn) {
      return parse_for_in();
    }
  }
  std::optional<Stmt> init;
  if (check(TokenType::KwLet)) {
    init = parse_let();
  } else {
    init = parse_update(true);
  }
  if (!init) {
    return std::nullopt;
  }
  std::optional<Expr> condition = parse_expression();
  if (!condition) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Stmt> step = parse_update(false);
  if (!step) {
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::For;
  stmt.expr = std::move(*condition);
  stmt.init.push_back(std::move(*init));
  stmt.step.push_back(std::move(*step));
  stmt.then_body = std::move(*body);
  return stmt;
}

std::optional<Stmt> Parser::parse_switch() {
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Expr> scrutinee = parse_expression();
  if (!scrutinee) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::Match;
  stmt.expr = std::move(*scrutinee);
  bool saw_default = false;
  ++m_block_depth;
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    Stmt::MatchArm arm;
    if (check(TokenType::KwCase)) {
      advance();
      std::optional<Expr> pattern = parse_expression();
      if (!pattern) {
        synchronize();
        break;
      }
      if (!consume(TokenType::Colon, "expected ':'")) {
        synchronize();
        break;
      }
      arm.pattern = std::move(*pattern);
    } else if (check(TokenType::KwDefault)) {
      advance();
      if (saw_default) {
        error("already declared");
        synchronize();
        break;
      }
      saw_default = true;
      if (!consume(TokenType::Colon, "expected ':'")) {
        synchronize();
        break;
      }
      arm.wildcard = true;
    } else {
      error("expected pattern");
      synchronize();
      break;
    }
    while (!check(TokenType::KwCase) && !check(TokenType::KwDefault) && !check(TokenType::CloseBrace) && !is_at_end()) {
      std::optional<Stmt> body = parse_statement();
      if (!body) {
        synchronize();
        if (check(TokenType::CloseBrace)) {
          break;
        }
        continue;
      }
      arm.body.push_back(std::move(*body));
    }
    stmt.arms.push_back(std::move(arm));
  }
  --m_block_depth;
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return std::nullopt;
  }
  if (!m_diagnostics.empty() && stmt.arms.empty()) {
    return std::nullopt;
  }
  return stmt;
}

std::optional<Stmt> Parser::parse_match() {
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return std::nullopt;
  }
  std::optional<Expr> scrutinee = parse_expression();
  if (!scrutinee) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return std::nullopt;
  }
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }

  Stmt stmt;
  stmt.kind = Stmt::Kind::Match;
  stmt.expr = std::move(*scrutinee);
  ++m_block_depth;
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    Stmt::MatchArm arm;
    const bool wildcard = check(TokenType::Identifier) && peek().lexeme == "_" &&
                          (peek_at(1).type == TokenType::SignalConnect || peek_at(1).type == TokenType::KwGuard);
    if (wildcard) {
      arm.wildcard = true;
      advance();
    } else if (check(TokenType::IntLiteral) || check(TokenType::FloatLiteral) || check(TokenType::Identifier) ||
               check(TokenType::StringLiteral) || check(TokenType::KwTrue) || check(TokenType::KwFalse) ||
               (check(TokenType::Minus) && (peek_at(1).type == TokenType::IntLiteral ||
                                            peek_at(1).type == TokenType::FloatLiteral))) {
      // literal patterns: 100, -1, 2.5, "play", true; identifiers are enum cases
      std::optional<Expr> pattern = parse_expression();
      if (!pattern) {
        synchronize();
        if (check(TokenType::CloseBrace)) {
          advance();
        }
        --m_block_depth;
        return std::nullopt;
      }
      arm.pattern = std::move(*pattern);
    } else {
      error("expected pattern");
      synchronize();
      if (check(TokenType::CloseBrace)) {
        advance();
      }
      --m_block_depth;
      return std::nullopt;
    }
    if (check(TokenType::KwGuard)) {
      advance();
      std::optional<Expr> guard = parse_expression();
      if (!guard) {
        synchronize();
        if (check(TokenType::CloseBrace)) {
          advance();
        }
        --m_block_depth;
        return std::nullopt;
      }
      arm.has_guard = true;
      arm.guard = std::move(*guard);
    }
    if (!consume(TokenType::SignalConnect, "expected '~>'")) {
      synchronize();
      if (check(TokenType::CloseBrace)) {
        advance();
      }
      --m_block_depth;
      return std::nullopt;
    }
    if (check(TokenType::OpenBrace)) {
      std::optional<std::vector<Stmt>> body = parse_block();
      if (!body) {
        --m_block_depth;
        return std::nullopt;
      }
      arm.body = std::move(*body);
    } else {
      std::optional<Stmt> body = parse_statement();
      if (!body) {
        synchronize();
        if (check(TokenType::CloseBrace)) {
          advance();
        }
        --m_block_depth;
        return std::nullopt;
      }
      arm.body.push_back(std::move(*body));
    }
    stmt.arms.push_back(std::move(arm));
  }
  --m_block_depth;
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return std::nullopt;
  }
  return stmt;
}

std::optional<Stmt> Parser::parse_return() {
  advance();
  Stmt stmt;
  stmt.kind = Stmt::Kind::Return;
  if (!check(TokenType::Semicolon)) {
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      synchronize();
      return std::nullopt;
    }
    stmt.returns_value = true;
    stmt.expr = std::move(*expr);
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  return stmt;
}

std::optional<VariantDecl> Parser::parse_variant() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }
  VariantDecl decl;
  decl.name = std::string(name.lexeme);
  decl.name_span = name.lexeme;
  decl.name_location = name.location;
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    if (!check(TokenType::Identifier) && !is_type_name(peek().type)) {
      error("expected type");
      synchronize();
      break;
    }
    decl.alternatives.emplace_back(peek().lexeme);
    advance();
    if (check(TokenType::Comma)) {
      advance();
    }
  }
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return std::nullopt;
  }
  if (decl.alternatives.empty()) {
    error("expected type");
    return std::nullopt;
  }
  return decl;
}

std::string Parser::parse_generic_suffix() {
  if (!check(TokenType::Less)) {
    return {};
  }
  advance();
  std::string text = "<";
  while (!is_at_end()) {
    const std::string arg = parse_type_atom();
    if (arg.empty()) {
      return {};
    }
    text += arg;
    if (!check(TokenType::Comma)) {
      break;
    }
    advance();
    text.push_back(',');
  }
  if (!consume(TokenType::Greater, "expected '>'")) {
    return {};
  }
  text.push_back('>');
  return text;
}

bool Parser::starts_generic_call() const {
  if (!check(TokenType::Less)) {
    return false;
  }
  int depth = 0;
  for (std::size_t index = 0;; ++index) {
    const TokenType type = peek_at(index).type;
    if (type == TokenType::Eof) {
      return false;
    }
    if (type == TokenType::Less) {
      ++depth;
    } else if (type == TokenType::Greater) {
      --depth;
      if (depth == 0) {
        return peek_at(index + 1).type == TokenType::OpenParen;
      }
    }
  }
}

std::string Parser::parse_type_atom() {
  if (check(TokenType::KwVoid)) {  // `-> void`
    advance();
    return "void";
  }
  if (check(TokenType::StringLiteral)) {
    const std::string text = "\"" + std::string(peek().lexeme) + "\"";
    advance();
    return text;
  }
  if (check(TokenType::Identifier) || is_type_name(peek().type)) {
    std::string text(peek().lexeme);
    advance();
    // Combat.Fighter: types from a linked module are shared by name, so the alias is dropped.
    while (check(TokenType::Dot) && peek_at(1).type == TokenType::Identifier) {
      advance();
      text = std::string(peek().lexeme);
      advance();
    }
    text += parse_generic_suffix();
    return text;
  }
  error("expected type");
  return {};
}

std::string Parser::parse_type_annotation() {
  const std::string first = parse_type_atom();
  if (first.empty()) {
    return {};
  }
  if (check(TokenType::Pipe)) {
    std::string text = first;
    while (check(TokenType::Pipe)) {
      advance();
      const std::string next = parse_type_atom();
      if (next.empty()) {
        return {};
      }
      text.push_back('|');
      text += next;
    }
    return text;
  }
  if (check(TokenType::Ampersand)) {
    std::string text = first;
    while (check(TokenType::Ampersand)) {
      advance();
      const std::string next = parse_type_atom();
      if (next.empty()) {
        return {};
      }
      text.push_back('&');
      text += next;
    }
    return text;
  }
  return first;
}

std::optional<Function> Parser::parse_extern() {
  advance();
  if (!consume(TokenType::KwFunc, "expected func")) {
    synchronize();
    return std::nullopt;
  }
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  Function function;
  function.name = std::string(name.lexeme);
  function.name_span = name.lexeme;
  function.name_location = name.location;
  function.is_extern = true;
  if (function.name == "strlen") {
    function.extern_id = 0;
    function.return_type = "int";
  } else {
    error("unknown extern");
  }
  if (!parse_parameter_list(function)) {
    return std::nullopt;
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return std::nullopt;
  }
  if (function.extern_id == 0 && function.name != "strlen") {
    return std::nullopt;
  }
  if (function.params.size() != 1) {
    error("wrong number of arguments");
    return std::nullopt;
  }
  return function;
}

std::optional<Function> Parser::parse_function() {
  advance();
  if (check(TokenType::KwOperator)) {
    const Token mark = peek();
    advance();
    std::string name = "operator";
    if (check(TokenType::Plus)) {
      name = "operator+";
    } else if (check(TokenType::Minus)) {
      name = "operator-";
    } else if (check(TokenType::OpenBracket)) {
      name = "operator[]";
    } else {
      error("expected operator");
      synchronize();
      return std::nullopt;
    }
    const Token sigil = peek();
    advance();
    if (name == "operator[]" && !consume(TokenType::CloseBracket, "expected ']'")) {
      synchronize();
      return std::nullopt;
    }
    Function function;
    function.name = std::move(name);
    function.name_span = sigil.lexeme;
    function.name_location = mark.location;
    if (!parse_parameter_list(function)) {
      return std::nullopt;
    }
    std::optional<std::vector<Stmt>> body = parse_block();
    if (!body) {
      return std::nullopt;
    }
    function.body = std::move(*body);
    return function;
  }
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  Function function;
  function.name = std::string(name.lexeme);
  function.name_span = name.lexeme;
  function.name_location = name.location;
  if (check(TokenType::Less)) {
    advance();
    while (check(TokenType::Identifier)) {
      function.type_params.emplace_back(peek().lexeme);
      advance();
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
    if (!consume(TokenType::Greater, "expected '>'")) {
      synchronize();
      return std::nullopt;
    }
  }
  if (!parse_parameter_list(function)) {
    return std::nullopt;
  }
  if (check(TokenType::Arrow)) {
    advance();
    function.return_type = parse_type_annotation();
  }
  if (check(TokenType::KwWhere)) {
    advance();
    while (check(TokenType::Identifier)) {
      function.where_names.emplace_back(peek().lexeme);
      advance();
      if (!consume(TokenType::Colon, "expected ':'")) {
        return std::nullopt;
      }
      function.where_types.push_back(parse_type_atom());
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  function.body = std::move(*body);
  return function;
}

std::optional<Function> Parser::parse_void_function() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  Function function;
  function.name = std::string(name.lexeme);
  function.name_span = name.lexeme;
  function.name_location = name.location;
  function.return_type = "void";
  if (!parse_parameter_list(function)) {
    return std::nullopt;
  }
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  function.body = std::move(*body);
  return function;
}

bool Parser::parse_link(Program& program) {
  advance();
  if (check(TokenType::At)) {
    const Token at = peek();
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      synchronize();
      return false;
    }
    std::string path = "@";
    path.append(peek().lexeme);
    advance();
    while (check(TokenType::Dot)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        return false;
      }
      path.push_back('.');
      path.append(peek().lexeme);
      advance();
    }
    // `link @clpp.axiom;` without `as` uses the last name, capitalized: Axiom.
    // (It used to be accepted and silently ignored.)
    std::string alias_name;
    std::string_view alias_span = at.lexeme;
    SourceLocation alias_location = at.location;
    if (check(TokenType::KwAs)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        return false;
      }
      alias_name = std::string(peek().lexeme);
      alias_span = peek().lexeme;
      alias_location = peek().location;
      advance();
    } else {
      alias_name = default_alias(path);
    }
    if (!consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return false;
    }
    if (name_taken(program, alias_name)) {
      m_diagnostics.push_back(Diagnostic{alias_location, alias_span, "already declared"});
      return false;
    }
    LinkDecl link;
    link.path = std::move(path);
    link.alias = std::move(alias_name);
    link.path_span = at.lexeme;
    link.path_location = at.location;
    link.alias_span = alias_span;
    link.alias_location = alias_location;
    program.links.push_back(std::move(link));
    return true;
  }
  if (!check(TokenType::StringLiteral)) {
    error("expected module");
    synchronize();
    return false;
  }
  const Token path = peek();
  advance();
  // `link "./combat.clp";` without `as` uses the file name, capitalized: Combat.
  std::string alias_name;
  std::string_view alias_span = path.lexeme;
  SourceLocation alias_location = path.location;
  if (check(TokenType::KwAs)) {
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      synchronize();
      return false;
    }
    alias_name = std::string(peek().lexeme);
    alias_span = peek().lexeme;
    alias_location = peek().location;
    advance();
  } else {
    alias_name = default_alias(std::string(path.lexeme));
  }
  if (!consume(TokenType::Semicolon, "expected ';'")) {
    synchronize();
    return false;
  }
  if (alias_name.empty()) {
    m_diagnostics.push_back(Diagnostic{path.location, path.lexeme, "cannot name this module; add `as Name`"});
    return false;
  }
  if (name_taken(program, alias_name)) {
    m_diagnostics.push_back(Diagnostic{alias_location, alias_span, "already declared"});
    return false;
  }
  LinkDecl link;
  link.path = std::string(path.lexeme);
  link.alias = std::move(alias_name);
  link.path_span = path.lexeme;
  link.path_location = path.location;
  link.alias_span = alias_span;
  link.alias_location = alias_location;
  program.links.push_back(std::move(link));
  return true;
}

std::optional<StructDecl> Parser::parse_struct() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  StructDecl decl;
  decl.name = std::string(name.lexeme);
  decl.name_span = name.lexeme;
  decl.name_location = name.location;
  if (check(TokenType::Less)) {
    advance();
    while (check(TokenType::Identifier)) {
      decl.type_params.emplace_back(peek().lexeme);
      advance();
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
    if (!consume(TokenType::Greater, "expected '>'")) {
      synchronize();
      return std::nullopt;
    }
  }
  if (check(TokenType::Colon)) {
    advance();
    while (check(TokenType::Identifier)) {
      if (decl.base.empty()) {
        decl.base = std::string(peek().lexeme);
        decl.base_span = peek().lexeme;
        decl.base_location = peek().location;
      }
      decl.bases.emplace_back(peek().lexeme);
      decl.base_spans.push_back(peek().lexeme);
      decl.base_locations.push_back(peek().location);
      advance();
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
    if (decl.bases.empty()) {
      error("expected name");
      synchronize();
      return std::nullopt;
    }
  }
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    bool is_private = false;
    bool is_override = false;
    bool is_abstract_method = false;
    bool is_final_method = false;
    while (check(TokenType::KwPrivate) || check(TokenType::KwPublic) || check(TokenType::KwOverride) ||
           check(TokenType::KwAbstract) || check(TokenType::KwFinal)) {
      if (check(TokenType::KwPrivate)) {
        is_private = true;
      } else if (check(TokenType::KwPublic)) {
        is_private = false;
      } else if (check(TokenType::KwOverride)) {
        is_override = true;
      } else if (check(TokenType::KwFinal)) {
        is_final_method = true;
      } else {
        is_abstract_method = true;
      }
      advance();
    }
    if (check(TokenType::KwFunc)) {
      std::optional<Function> method = parse_function();
      if (!method) {
        return std::nullopt;
      }
      bool duplicate = false;
      const std::string qualified = decl.name + "." + method->name;
      for (const Function& existing : decl.methods) {
        if (existing.name == qualified) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        m_diagnostics.push_back(Diagnostic{method->name_location, method->name_span, "already declared"});
        continue;
      }
      method->name = qualified;
      method->is_private = is_private;
      method->is_final = is_final_method;
      method->is_override = is_override;
      method->is_abstract = is_abstract_method;
      method->params.insert(method->params.begin(), "self");
      method->param_types.insert(method->param_types.begin(), decl.name);
      method->param_spans.insert(method->param_spans.begin(), decl.name_span);
      method->param_locations.insert(method->param_locations.begin(), decl.name_location);
      if (is_abstract_method) {
        method->body.clear();
        Stmt result;
        result.kind = Stmt::Kind::Return;
        result.returns_value = true;
        result.expr.kind = Expr::Kind::Number;
        method->body.push_back(std::move(result));
      }
      decl.methods.push_back(std::move(*method));
      continue;
    }
    if (!is_type_name(peek().type) && !check(TokenType::Identifier)) {
      error("expected type");
      synchronize();
      return std::nullopt;
    }
    const std::string type_name(peek().lexeme);
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      synchronize();
      return std::nullopt;
    }
    const Token field_name = peek();
    advance();
    bool duplicate = false;
    for (const Field& field : decl.fields) {
      if (field.name == field_name.lexeme) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      m_diagnostics.push_back(Diagnostic{field_name.location, field_name.lexeme, "already declared"});
    } else {
      Field field;
      field.name = std::string(field_name.lexeme);
      field.type_name = type_name;
      field.owner = decl.name;
      field.is_private = is_private;
      field.name_span = field_name.lexeme;
      field.name_location = field_name.location;
      decl.fields.push_back(std::move(field));
    }
    if (!consume(TokenType::Semicolon, "expected ';'")) {
      synchronize();
      return std::nullopt;
    }
  }
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return std::nullopt;
  }
  return decl;
}

std::optional<EnumDecl> Parser::parse_enum() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  EnumDecl decl;
  decl.name = std::string(name.lexeme);
  decl.name_span = name.lexeme;
  decl.name_location = name.location;
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }
  if (!check(TokenType::CloseBrace)) {
    while (true) {
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        return std::nullopt;
      }
      const Token variant = peek();
      advance();
      std::string payload;
      if (check(TokenType::OpenParen)) {
        advance();
        payload = parse_type_atom();
        if (payload.empty() || !consume(TokenType::CloseParen, "expected ')'")) {
          synchronize();
          return std::nullopt;
        }
      }
      bool duplicate = false;
      for (const std::string& existing : decl.variants) {
        if (existing == variant.lexeme) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        m_diagnostics.push_back(Diagnostic{variant.location, variant.lexeme, "already declared"});
      } else {
        decl.variants.emplace_back(variant.lexeme);
        decl.payload_types.push_back(std::move(payload));
        decl.variant_spans.push_back(variant.lexeme);
        decl.variant_locations.push_back(variant.location);
      }
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
      if (check(TokenType::CloseBrace)) {
        break;
      }
    }
  }
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return std::nullopt;
  }
  return decl;
}

bool Parser::parse_parameter_list(Function& function) {
  if (!consume(TokenType::OpenParen, "expected '('")) {
    synchronize();
    return false;
  }
  if (!check(TokenType::CloseParen)) {
    while (true) {
      std::string param_type;
      // int x, Fighter f, Combat.Fighter f, array<int> xs
      if (is_type_name(peek().type) ||
          (check(TokenType::Identifier) &&
           (peek_at(1).type == TokenType::Identifier || peek_at(1).type == TokenType::Less ||
            (peek_at(1).type == TokenType::Dot && peek_at(2).type == TokenType::Identifier &&
             peek_at(3).type == TokenType::Identifier)))) {
        param_type = parse_type_atom();
      }
      if (check(TokenType::Ellipsis)) {
        function.variadic = true;
        advance();
      }
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        return false;
      }
      const Token param = peek();
      function.params.emplace_back(param.lexeme);
      function.param_types.push_back(std::move(param_type));
      function.param_spans.push_back(param.lexeme);
      function.param_locations.push_back(param.location);
      function.has_default.push_back(0);
      function.defaults.emplace_back();
      advance();
      if (check(TokenType::Equal)) {
        advance();
        std::optional<Expr> fallback = parse_expression();
        if (!fallback) {
          synchronize();
          return false;
        }
        function.defaults.back() = std::move(*fallback);
        function.has_default.back() = 1;
      }
      if (!check(TokenType::Comma)) {
        break;
      }
      advance();
    }
  }
  if (!consume(TokenType::CloseParen, "expected ')'")) {
    synchronize();
    return false;
  }
  return true;
}

std::optional<std::vector<Stmt>> Parser::parse_block() {
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }

  ++m_block_depth;
  std::vector<Stmt> body;
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    if (check(TokenType::DocComment) || check(TokenType::Invalid)) {
      advance();
      continue;
    }
    const std::size_t before = m_current;
    if (std::optional<Stmt> stmt = parse_statement()) {
      body.push_back(std::move(*stmt));
    } else if (m_current == before) {
      advance();
    }
  }
  --m_block_depth;
  if (!consume(TokenType::CloseBrace, "expected '}'")) {
    return body;
  }
  return body;
}

std::optional<Expr> Parser::parse_expression() {
  std::optional<Expr> expr = parse_or();
  if (!expr) {
    return std::nullopt;
  }
  if (!check(TokenType::Question)) {
    return expr;
  }
  advance();
  std::optional<Expr> then_expr = parse_expression();
  if (!then_expr || !consume(TokenType::Colon, "expected ':'")) {
    return std::nullopt;
  }
  std::optional<Expr> else_expr = parse_expression();
  if (!else_expr) {
    return std::nullopt;
  }
  Expr ternary;
  ternary.kind = Expr::Kind::Ternary;
  ternary.left = std::make_unique<Expr>(std::move(*expr));
  ternary.right = std::make_unique<Expr>(std::move(*then_expr));
  ternary.args.push_back(std::move(*else_expr));
  return ternary;
}

std::optional<Expr> Parser::parse_or() {
  std::optional<Expr> expr = parse_and();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::KwOr)) {
    advance();
    std::optional<Expr> right = parse_and();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(Expr::Kind::Or, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_and() {
  std::optional<Expr> expr = parse_bitor();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::KwAnd)) {
    advance();
    std::optional<Expr> right = parse_bitor();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(Expr::Kind::And, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_bitor() {
  std::optional<Expr> expr = parse_bitxor();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::Pipe)) {
    advance();
    std::optional<Expr> right = parse_bitxor();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(Expr::Kind::BitOr, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_bitxor() {
  std::optional<Expr> expr = parse_bitand();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::Caret)) {
    advance();
    std::optional<Expr> right = parse_bitand();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(Expr::Kind::BitXor, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_bitand() {
  std::optional<Expr> expr = parse_shift();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::Ampersand)) {
    advance();
    std::optional<Expr> right = parse_shift();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(Expr::Kind::BitAnd, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_shift() {
  std::optional<Expr> expr = parse_unary();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::KwShl) || check(TokenType::KwShr)) {
    const Expr::Kind kind = check(TokenType::KwShl) ? Expr::Kind::Shl : Expr::Kind::Shr;
    advance();
    std::optional<Expr> right = parse_unary();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_unary() {
  if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
    const bool increment = check(TokenType::PlusPlus);
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner || inner->kind != Expr::Kind::Name) {
      error("expected name");
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::Update;
    expr.number = increment ? 1 : -1;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    return expr;
  }
  if (check(TokenType::BitNot)) {
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::BitNot;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    return expr;
  }
  if (check(TokenType::Minus)) {
    const Token token = peek();
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner) {
      return std::nullopt;
    }
    Expr zero;
    zero.kind = Expr::Kind::Number;
    zero.number = 0;
    zero.location = token.location;
    zero.span = token.lexeme;
    return make_binary(Expr::Kind::Sub, std::move(zero), std::move(*inner));
  }
  if (check(TokenType::Bang)) {
    const Token token = peek();
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::Not;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    expr.location = token.location;
    expr.span = token.lexeme;
    return expr;
  }
  if (check(TokenType::KwThread)) {
    const Token token = peek();
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner || inner->kind != Expr::Kind::Call) {
      error("expected call");
      return std::nullopt;
    }
    inner->thread_call = true;
    inner->location = token.location;
    inner->span = token.lexeme;
    return inner;
  }
  if (check(TokenType::KwNew)) {
    advance();
    return parse_unary();
  }
  if (check(TokenType::KwSpawn) || check(TokenType::KwAwait)) {
    const Token token = peek();
    const bool spawn = check(TokenType::KwSpawn);
    advance();
    std::optional<Expr> inner = parse_unary();
    if (!inner) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = spawn ? Expr::Kind::Spawn : Expr::Kind::Await;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    expr.location = token.location;
    expr.span = token.lexeme;
    return expr;
  }
  if (!check(TokenType::KwNot)) {
    return parse_comparison();
  }
  advance();
  std::optional<Expr> inner = parse_unary();
  if (!inner) {
    return std::nullopt;
  }
  Expr expr;
  expr.kind = Expr::Kind::Not;
  expr.left = std::make_unique<Expr>(std::move(*inner));
  return expr;
}

std::optional<Expr> Parser::parse_comparison() {
  std::optional<Expr> expr = parse_additive();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::EqualEqual) || check(TokenType::NotEqual) || check(TokenType::LessEqual) ||
         check(TokenType::GreaterEqual) || check(TokenType::Less) || check(TokenType::Greater)) {
    Expr::Kind kind = Expr::Kind::Less;
    if (check(TokenType::EqualEqual)) {
      kind = Expr::Kind::Eq;
    } else if (check(TokenType::NotEqual)) {
      kind = Expr::Kind::NotEq;
    } else if (check(TokenType::LessEqual)) {
      kind = Expr::Kind::LessEq;
    } else if (check(TokenType::GreaterEqual)) {
      kind = Expr::Kind::GreaterEq;
    } else if (check(TokenType::Greater)) {
      kind = Expr::Kind::Greater;
    }
    advance();
    std::optional<Expr> right = parse_additive();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_additive() {
  std::optional<Expr> expr = parse_multiplicative();
  if (!expr) {
    return std::nullopt;
  }

  while (check(TokenType::Plus) || check(TokenType::Minus) || check(TokenType::Concat) || check(TokenType::DotDot)) {
    Expr::Kind kind = Expr::Kind::Add;
    if (check(TokenType::Minus)) {
      kind = Expr::Kind::Sub;
    } else if (check(TokenType::Concat)) {
      kind = Expr::Kind::Concat;
    } else if (check(TokenType::DotDot)) {
      kind = Expr::Kind::Range;
    }
    advance();
    std::optional<Expr> right = parse_multiplicative();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }

  return expr;
}

std::optional<Expr> Parser::parse_multiplicative() {
  std::optional<Expr> expr = parse_primary();
  if (!expr) {
    return std::nullopt;
  }

  while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Percent)) {
    Expr::Kind kind = Expr::Kind::Mul;
    if (check(TokenType::Slash)) {
      kind = Expr::Kind::Div;
    } else if (check(TokenType::Percent)) {
      kind = Expr::Kind::Mod;
    }
    advance();
    std::optional<Expr> right = parse_primary();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }

  return expr;
}

std::optional<Expr> Parser::parse_self_field() {
  advance();
  if (!check(TokenType::Identifier)) {
    error("expected name");
    return std::nullopt;
  }
  Token name = peek();
  advance();
  if (name.lexeme == "this") {
    if (check(TokenType::Scope)) {
      advance();
    } else if (!consume(TokenType::Dot, "expected '.'")) {
      return std::nullopt;
    }
    if (!check(TokenType::Identifier)) {
      error("expected name");
      return std::nullopt;
    }
    name = peek();
    advance();
  }
  Expr expr;
  expr.kind = Expr::Kind::SelfField;
  expr.value = std::string(name.lexeme);
  expr.span = name.lexeme;
  expr.location = name.location;
  return expr;
}

std::optional<Expr> Parser::parse_primary() {
  if (check(TokenType::KwFunc) && peek_at(1).type == TokenType::OpenParen && m_program != nullptr) {
    const Token mark = peek();
    advance();
    Function function;
    function.name = "anon" + std::to_string(m_program->functions.size());
    function.name_span = mark.lexeme;
    function.name_location = mark.location;
    if (!parse_parameter_list(function)) {
      return std::nullopt;
    }
    std::optional<std::vector<Stmt>> body = parse_block();
    if (!body) {
      return std::nullopt;
    }
    function.body = std::move(*body);
    Expr named;
    named.kind = Expr::Kind::Name;
    named.value = function.name;
    named.span = mark.lexeme;
    named.location = mark.location;
    m_program->functions.push_back(std::move(function));
    return named;
  }

  if (check(TokenType::KwJoin)) {
    const Token token = peek();
    advance();
    if (!consume(TokenType::OpenParen, "expected '('")) {
      return std::nullopt;
    }
    std::optional<Expr> inner = parse_expression();
    if (!inner) {
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::Join;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    expr.location = token.location;
    expr.span = token.lexeme;
    return expr;
  }

  if (check(TokenType::KwMove)) {
    const Token token = peek();
    advance();
    if (!consume(TokenType::OpenParen, "expected '('")) {
      return std::nullopt;
    }
    std::optional<Expr> inner = parse_expression();
    if (!inner) {
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::MoveFrom;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    expr.location = token.location;
    expr.span = token.lexeme;
    return expr;
  }

  if (check(TokenType::KwEndl)) {
    const Token token = peek();
    Expr expr;
    expr.kind = Expr::Kind::String;
    expr.location = token.location;
    expr.span = token.lexeme;
    advance();
    return expr;
  }

  if (check(TokenType::KwPcall)) {
    const Token token = peek();
    advance();
    if (!consume(TokenType::OpenParen, "expected '('")) {
      return std::nullopt;
    }
    std::optional<Expr> inner = parse_expression();
    if (!inner) {
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::PCall;
    expr.left = std::make_unique<Expr>(std::move(*inner));
    expr.location = token.location;
    expr.span = token.lexeme;
    return expr;
  }

  if (check(TokenType::KwTrue) || check(TokenType::KwFalse) || check(TokenType::KwNull)) {
    const Token token = peek();
    Expr expr;
    expr.kind = Expr::Kind::Number;
    expr.number = token.type == TokenType::KwTrue ? 1 : 0;
    expr.bool_literal = token.type != TokenType::KwNull;
    expr.null_literal = token.type == TokenType::KwNull;
    expr.location = token.location;
    expr.span = token.lexeme;
    advance();
    return expr;
  }

  if (check(TokenType::KwParallel)) {
    const Token token = peek();
    advance();
    if (!consume(TokenType::OpenParen, "expected '('")) {
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::Parallel;
    expr.location = token.location;
    expr.span = token.lexeme;
    if (!check(TokenType::CloseParen)) {
      while (true) {
        std::optional<Expr> arg = parse_expression();
        if (!arg) {
          return std::nullopt;
        }
        expr.args.push_back(std::move(*arg));
        if (!check(TokenType::Comma)) {
          break;
        }
        advance();
      }
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    if (expr.args.size() < 2) {
      m_diagnostics.push_back(Diagnostic{token.location, token.lexeme, "wrong number of arguments"});
      return std::nullopt;
    }
    return expr;
  }

  if (check(TokenType::At)) {
    return parse_self_field();
  }

  if (check(TokenType::TemplateString) || check(TokenType::TemplateHead)) {
    return parse_template();
  }

  if (check(TokenType::StringLiteral)) {
    const std::optional<std::string> decoded = decode_escapes(peek().lexeme);
    if (!decoded) {
      error("invalid escape");
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::String;
    expr.value = *decoded;
    expr.location = peek().location;
    expr.span = peek().lexeme;
    advance();
    return finish_postfix(std::move(expr));
  }

  if (check(TokenType::IntLiteral) || check(TokenType::FloatLiteral)) {
    double number = 0;
    if (!parse_number_lexeme(peek().lexeme, number)) {
      error("invalid numeric literal");
      return std::nullopt;
    }
    Expr expr;
    expr.kind = Expr::Kind::Number;
    expr.number = number;
    expr.float_literal = check(TokenType::FloatLiteral);
    expr.location = peek().location;
    expr.span = peek().lexeme;
    advance();
    return finish_postfix(std::move(expr));
  }

  if (is_callable_name(peek().type) && peek_at(1).type == TokenType::FatArrow && m_program != nullptr) {
    const Token param = peek();
    advance();
    advance();
    Function function;
    function.name = "anon" + std::to_string(m_program->functions.size());
    function.params.emplace_back(param.lexeme);
    function.param_types.emplace_back();
    function.param_spans.push_back(param.lexeme);
    function.param_locations.push_back(param.location);
    if (check(TokenType::OpenBrace)) {
      std::optional<std::vector<Stmt>> body = parse_block();
      if (!body) {
        return std::nullopt;
      }
      function.body = std::move(*body);
    } else {
      std::optional<Expr> body = parse_expression();
      if (!body) {
        return std::nullopt;
      }
      Stmt result;
      result.kind = Stmt::Kind::Return;
      result.returns_value = true;
      result.expr = std::move(*body);
      function.body.push_back(std::move(result));
    }
    Expr named;
    named.kind = Expr::Kind::Name;
    named.value = function.name;
    named.span = param.lexeme;
    named.location = param.location;
    m_program->functions.push_back(std::move(function));
    return named;
  }

  if (is_callable_name(peek().type)) {
    const Token name = peek();
    advance();
    std::string callable(name.lexeme);
    if (starts_generic_call()) {
      callable += parse_generic_suffix();
    }
    if (check(TokenType::Scope)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        return std::nullopt;
      }
      callable.append("::");
      callable.append(peek().lexeme);
      advance();
    }
    if (check(TokenType::Dot) && peek_at(1).type == TokenType::Identifier &&
        peek_at(2).type == TokenType::OpenParen) {
      advance();
      callable.push_back('.');
      callable.append(peek().lexeme);
      advance();
    }
    if (!check(TokenType::OpenParen)) {
      Expr named;
      named.kind = Expr::Kind::Name;
      named.value = std::move(callable);
      named.span = name.lexeme;
      named.location = name.location;
      return finish_postfix(std::move(named));
    }
    advance();
    Expr call;
    call.kind = Expr::Kind::Call;
    call.value = std::move(callable);
    call.span = name.lexeme;
    call.location = name.location;
    if (!check(TokenType::CloseParen)) {
      while (true) {
        std::string arg_name;
        if (check(TokenType::Identifier) && peek_at(1).type == TokenType::Colon) {
          arg_name = std::string(peek().lexeme);
          advance();
          advance();
        }
        std::optional<Expr> arg = parse_expression();
        if (!arg) {
          return std::nullopt;
        }
        call.arg_names.push_back(std::move(arg_name));
        call.args.push_back(std::move(*arg));
        if (!check(TokenType::Comma)) {
          break;
        }
        advance();
      }
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    return finish_postfix(std::move(call));
  }

  if (check(TokenType::OpenParen)) {
    advance();
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    return finish_postfix(std::move(*expr));
  }

  error("expected expression");
  return std::nullopt;
}

std::optional<Expr> Parser::parse_embedded_expression() {
  m_diagnostics.assign(m_lexer_diagnostics.begin(), m_lexer_diagnostics.end());
  std::optional<Expr> expr = parse_expression();
  if (!expr) {
    return std::nullopt;
  }
  if (!is_at_end()) {
    error("expected expression");
    return std::nullopt;
  }
  return expr;
}

std::optional<Expr> Parser::parse_template() {
  if (check(TokenType::TemplateHead)) {
    std::vector<Expr> parts;
    const auto push_literal = [&](const Token& piece) {
      if (piece.lexeme.empty()) {
        return true;
      }
      const std::optional<std::string> decoded = decode_escapes(piece.lexeme);
      if (!decoded) {
        error("invalid escape");
        return false;
      }
      Expr literal;
      literal.kind = Expr::Kind::String;
      literal.value = *decoded;
      literal.location = piece.location;
      literal.span = piece.lexeme;
      parts.push_back(std::move(literal));
      return true;
    };
    const Token head = peek();
    advance();
    if (!push_literal(head)) {
      return std::nullopt;
    }
    while (true) {
      std::optional<Expr> piece = parse_expression();
      if (!piece) {
        return std::nullopt;
      }
      parts.push_back(std::move(*piece));
      if (check(TokenType::TemplateMiddle)) {
        const Token middle = peek();
        advance();
        if (!push_literal(middle)) {
          return std::nullopt;
        }
        continue;
      }
      if (!check(TokenType::TemplateTail)) {
        error("unterminated interpolation");
        return std::nullopt;
      }
      const Token tail = peek();
      advance();
      if (!push_literal(tail)) {
        return std::nullopt;
      }
      break;
    }
    if (parts.empty()) {
      Expr empty;
      empty.kind = Expr::Kind::String;
      empty.location = head.location;
      empty.span = head.lexeme;
      return empty;
    }
    Expr result = std::move(parts[0]);
    if (result.kind != Expr::Kind::String) {
      Expr empty;
      empty.kind = Expr::Kind::String;
      empty.location = head.location;
      empty.span = head.lexeme;
      result = make_binary(Expr::Kind::Concat, std::move(empty), std::move(result));
    }
    for (std::size_t part = 1; part < parts.size(); ++part) {
      result = make_binary(Expr::Kind::Concat, std::move(result), std::move(parts[part]));
    }
    result.location = head.location;
    result.span = head.lexeme;
    return result;
  }
  const Token token = peek();
  const std::string_view text = token.lexeme;
  std::vector<Expr> parts;
  std::string literal;

  const auto flush_literal = [&]() {
    if (literal.empty()) {
      return;
    }
    Expr piece;
    piece.kind = Expr::Kind::String;
    piece.value = std::move(literal);
    piece.location = token.location;
    piece.span = token.lexeme;
    parts.push_back(std::move(piece));
    literal.clear();
  };

  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\\' && index + 1 < text.size()) {
      const std::optional<std::string> decoded = decode_escapes(text.substr(index, 2));
      if (!decoded) {
        error("invalid escape");
        return std::nullopt;
      }
      literal += *decoded;
      ++index;
      continue;
    }
    if (text[index] == '$' && index + 1 < text.size() && text[index + 1] == '{') {
      flush_literal();
      const std::size_t close = find_interpolation_end(text, index + 1);
      if (close == std::string_view::npos) {
        SourceLocation location = token.location;
        location.column += static_cast<std::uint32_t>(index + 1);
        m_diagnostics.push_back(Diagnostic{location, text.substr(index), "unterminated interpolation"});
        return std::nullopt;
      }
      const std::string_view hole = text.substr(index + 2, close - (index + 2));
      const auto hole_column = token.location.column + static_cast<std::uint32_t>(index + 3);
      Lexer lexer(hole);
      LexResult lexed = lexer.tokenize();
      for (Token& hole_token : lexed.tokens) {
        hole_token.location.line = token.location.line;
        hole_token.location.column += hole_column - 1;
      }
      for (Diagnostic& diagnostic : lexed.diagnostics) {
        diagnostic.location.line = token.location.line;
        diagnostic.location.column += hole_column - 1;
      }
      Parser embedded(lexed.tokens, lexed.diagnostics);
      std::optional<Expr> piece = embedded.parse_embedded_expression();
      m_diagnostics.insert(m_diagnostics.end(), embedded.m_diagnostics.begin(), embedded.m_diagnostics.end());
      if (!piece) {
        return std::nullopt;
      }
      parts.push_back(std::move(*piece));
      index = close;
      continue;
    }
    literal.push_back(text[index]);
  }
  flush_literal();

  Expr result;
  if (parts.empty()) {
    result.kind = Expr::Kind::String;
    result.location = token.location;
    result.span = token.lexeme;
    advance();
    return result;
  }

  result = std::move(parts[0]);
  if (result.kind != Expr::Kind::String) {
    Expr empty;
    empty.kind = Expr::Kind::String;
    empty.location = token.location;
    empty.span = token.lexeme;
    result = make_binary(Expr::Kind::Concat, std::move(empty), std::move(result));
  }
  for (std::size_t part = 1; part < parts.size(); ++part) {
    result = make_binary(Expr::Kind::Concat, std::move(result), std::move(parts[part]));
  }
  result.location = token.location;
  result.span = token.lexeme;
  advance();
  return result;
}

std::optional<Expr> Parser::finish_postfix(Expr expr) {
  while (check(TokenType::Dot) || check(TokenType::OpenBracket) || check(TokenType::PlusPlus) ||
         check(TokenType::MinusMinus) || (expr.kind == Expr::Kind::Name && check(TokenType::OpenParen))) {
    if (expr.kind == Expr::Kind::Name && check(TokenType::OpenParen)) {
      advance();
      Expr call;
      call.kind = Expr::Kind::Call;
      call.value = expr.value;
      call.span = expr.span;
      call.location = expr.location;
      if (!check(TokenType::CloseParen)) {
        while (true) {
          std::optional<Expr> arg = parse_expression();
          if (!arg) {
            return std::nullopt;
          }
          call.args.push_back(std::move(*arg));
          if (!check(TokenType::Comma)) {
            break;
          }
          advance();
        }
      }
      if (!consume(TokenType::CloseParen, "expected ')'")) {
        return std::nullopt;
      }
      expr = std::move(call);
      continue;
    }
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
      Expr update;
      update.kind = Expr::Kind::Update;
      update.postfix = true;
      update.number = check(TokenType::PlusPlus) ? 1 : -1;
      update.left = std::make_unique<Expr>(std::move(expr));
      advance();
      expr = std::move(update);
      continue;
    }
    if (check(TokenType::OpenBracket)) {
      const Token token = peek();
      advance();
      std::optional<Expr> index = parse_expression();
      if (!index) {
        return std::nullopt;
      }
      if (!consume(TokenType::CloseBracket, "expected ']'")) {
        return std::nullopt;
      }
      Expr indexed;
      indexed.kind = Expr::Kind::Index;
      indexed.left = std::make_unique<Expr>(std::move(expr));
      indexed.right = std::make_unique<Expr>(std::move(*index));
      indexed.location = token.location;
      indexed.span = token.lexeme;
      expr = std::move(indexed);
      continue;
    }
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      Expr member;
      member.kind = Expr::Kind::Member;
      member.missing = true;
      member.location = peek().location;
      member.span = peek().lexeme;
      member.left = std::make_unique<Expr>(std::move(expr));
      return member;
    }
    Expr member;
    member.kind = Expr::Kind::Member;
    member.value = std::string(peek().lexeme);
    member.span = peek().lexeme;
    member.location = peek().location;
    member.left = std::make_unique<Expr>(std::move(expr));
    advance();
    expr = std::move(member);
  }
  return expr;
}

}  // namespace clpp::parser
