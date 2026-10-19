#include "clpp/numeric.hpp"
#include "core/parser/parser.hpp"

#include <algorithm>

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

struct AnnotationTypeScope {
  std::vector<std::string>& names;
  std::size_t size;
  AnnotationTypeScope(std::vector<std::string>& storage, const std::vector<std::string>& added)
      : names(storage), size(storage.size()) { names.insert(names.end(), added.begin(), added.end()); }
  ~AnnotationTypeScope() { names.resize(size); }
};

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
         type == TokenType::KwBuffer || type == TokenType::KwNumber || type == TokenType::KwFloat ||
         type == TokenType::KwString || type == TokenType::KwBool ||
         type == TokenType::KwTask || type == TokenType::KwSignal;
}

[[nodiscard]] bool is_callable_name(const TokenType type) {
  return type == TokenType::Identifier || type == TokenType::KwStaticCast || is_type_name(type);
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

[[nodiscard]] Expr raw_binary(const Expr::Kind kind, Expr left, Expr right) {
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

[[nodiscard]] bool function_slot_taken(const Program& program, const std::string_view name) {
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
  for (const LinkDecl& link : program.links) {
    if (link.alias == name) {
      return true;
    }
  }
  for (const Function& function : program.functions) {
    if (function.name == name) {
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

namespace {
struct SyntaxDepth {
  std::size_t& depth;
  explicit SyntaxDepth(std::size_t& value) : depth(value) { ++depth; }
  ~SyntaxDepth() { --depth; }
};
}

bool Parser::nesting_available(const std::size_t depth) {
  // Keep malformed input below the smallest supported native stack budget.
  // Stop consuming this source after the limit; recovery must not recurse again.
  if (depth < 80) return true;
  error("syntax nesting limit exceeded");
  m_current = m_tokens.size() - 1;
  return false;
}

bool Parser::expression_depth_valid(const Expr& expression) {
  if (!expression.left && !expression.right && expression.args.empty()) return true;
  std::vector<std::pair<const Expr*, std::size_t>> pending{{&expression, 0}};
  while (!pending.empty()) {
    const auto [node, depth] = pending.back();
    pending.pop_back();
    if (depth > 128) {
      error("syntax nesting limit exceeded");
      m_current = m_tokens.size() - 1;
      return false;
    }
    if (node->left) pending.emplace_back(node->left.get(), depth + 1);
    if (node->right) pending.emplace_back(node->right.get(), depth + 1);
    for (const auto& argument : node->args) pending.emplace_back(&argument, depth + 1);
  }
  return true;
}

Expr Parser::make_binary(const Expr::Kind kind, Expr left, Expr right) {
  Expr expression = raw_binary(kind, std::move(left), std::move(right));
  if (expression_depth_valid(expression)) return expression;
  Expr missing;
  missing.missing = true;
  return missing;
}

ParseResult Parser::parse() {
  m_diagnostics.assign(m_lexer_diagnostics.begin(), m_lexer_diagnostics.end());
  Program program;
  m_program = &program;

  bool mode_set = false;
  while (!is_at_end()) {
    const bool exported = check(TokenType::KwExport);
    if (exported) {
      advance();
      const bool data_modifier = (check(TokenType::KwFinal) || check(TokenType::KwAbstract)) &&
                                peek_at(1).type == TokenType::KwDef;
      if (!check(TokenType::KwDef) && !check(TokenType::KwConst) &&
          !(check(TokenType::KwEnum) && peek_at(1).type == TokenType::KwClass) &&
          !check(TokenType::KwAsync) && !check(TokenType::KwTemplate) && !check(TokenType::KwConstexpr) && !data_modifier && !starts_typed_function()) {
        error("export requires a def, scoped enum, typed function or constant declaration");
        synchronize();
        continue;
      }
    }
    if (check(TokenType::KwPragma)) {
      const bool after_declaration = !program.statements.empty() || !program.functions.empty() ||
          !program.structs.empty() || !program.enums.empty() || !program.aliases.empty() || !program.links.empty();
      if (mode_set || after_declaration) {
        error("pragma must appear once before declarations");
      }
      advance();
      if (check(TokenType::Identifier) && (peek().lexeme == "strict" || peek().lexeme == "nostrict")) {
        program.mode = peek().lexeme == "strict" ? CheckMode::Strict : CheckMode::Nonstrict;
        mode_set = true;
        advance();
        if (!consume(TokenType::Semicolon, "expected ';'")) synchronize();
      } else {
        error("unknown pragma mode");
        synchronize();
      }
      continue;
    }
    if (check(TokenType::KwVariant) || check(TokenType::KwNamespace) || check(TokenType::KwType) ||
        check(TokenType::KwImport) || check(TokenType::KwFrom) || check(TokenType::KwStatic)) {
      error("obsolete declaration syntax; use def, typed functions and link modules");
      synchronize(); continue;
    }
    if (check(TokenType::KwStruct) || ((check(TokenType::KwFinal) || check(TokenType::KwAbstract)) && peek_at(1).type == TokenType::KwStruct)) {
      error("data declarations use def; struct is not supported");
      synchronize();
      continue;
    }
    if (check(TokenType::Invalid)) {
      advance();
      continue;
    }
    if (check(TokenType::KwTry)) {
      if (std::optional<Stmt> stmt = parse_try()) {
        program.statements.push_back(std::move(*stmt));
      }
      continue;
    }
    if (check(TokenType::KwFinal) && peek_at(1).type == TokenType::KwDef) {
      advance();
      if (std::optional<StructDecl> decl = parse_struct()) {
        decl->is_final = true;
        decl->exported = exported;
        if (!name_taken(program, decl->name)) {
          for (Function& method : decl->methods) {
            program.functions.push_back(std::move(method));
          }
          program.structs.push_back(std::move(*decl));
        }
      }
      continue;
    }
    if (check(TokenType::KwLink)) {
      (void)parse_link(program);
      continue;
    }
    if (check(TokenType::KwTemplate)) {
      if (auto function = parse_generic_function()) {
        function->exported = exported;
        if (function_slot_taken(program, function->name)) {
          m_diagnostics.push_back(Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (starts_typed_function()) {
      if (std::optional<Function> function = parse_typed_function()) {
        function->exported = exported;
        if (function_slot_taken(program, function->name)) {
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
      if (!starts_typed_function()) {
        error("expected typed function");
        synchronize();
        continue;
      }
      if (std::optional<Function> function = parse_typed_function()) {
        function->is_async = true;
        function->exported = exported;
        if (function_slot_taken(program, function->name)) {
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
        if (function_slot_taken(program, function->name)) {
          m_diagnostics.push_back(Diagnostic{function->name_location, function->name_span, "already declared"});
        } else {
          program.functions.push_back(std::move(*function));
        }
      }
      continue;
    }
    if (check(TokenType::KwFunc)) {
      error("named functions use return-type-first declarations; func is only for lambdas");
      synchronize();
      continue;
    }
    if (check(TokenType::KwAbstract) && peek_at(1).type == TokenType::KwDef) {
      advance();
      if (std::optional<StructDecl> decl = parse_struct()) {
        decl->is_abstract = true;
        decl->exported = exported;
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
    std::size_t alias_equal = 2;
    if (check(TokenType::KwDef) && peek_at(alias_equal).type == TokenType::Less) {
      while (peek_at(alias_equal).type != TokenType::Greater && peek_at(alias_equal).type != TokenType::Eof) ++alias_equal;
      ++alias_equal;
    }
    if (check(TokenType::KwDef) && peek_at(alias_equal).type == TokenType::Equal) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        synchronize();
        continue;
      }
      const Token name = peek();
      advance();
      std::vector<std::string> type_params;
      if (check(TokenType::Less)) {
        advance();
        do {
          if (!check(TokenType::Identifier)) { error("expected type parameter"); break; }
          type_params.emplace_back(peek().lexeme); advance();
          if (!check(TokenType::Comma)) break;
          advance();
        } while (!is_at_end());
        if (!consume(TokenType::Greater, "expected '>'")) { synchronize(); continue; }
      }
      if (!consume(TokenType::Equal, "expected '='")) { synchronize(); continue; }
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
      program.aliases.push_back(TypeAlias{std::string(name.lexeme), body, exported, std::move(type_params)});
      continue;
    }
    if (check(TokenType::KwDef)) {
      if (std::optional<StructDecl> decl = parse_struct()) {
        decl->exported = exported;
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
    if (check(TokenType::KwEnum)) {
      if (std::optional<EnumDecl> decl = parse_enum()) {
        decl->exported = exported;
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
      if (exported && (stmt->kind != Stmt::Kind::VariableDecl || !stmt->immutable)) {
        m_diagnostics.push_back(Diagnostic{stmt->name_location, stmt->name_span,
                                           "export requires a constant declaration"});
      } else {
        stmt->exported = exported;
        program.statements.push_back(std::move(*stmt));
      }
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
  if (!nesting_available(m_statement_depth)) return std::nullopt;
  const SyntaxDepth statement_depth(m_statement_depth);
  if (check(TokenType::KwPost) || check(TokenType::KwWarn) || check(TokenType::KwReport)) {
    return parse_post();
  }
  if (check(TokenType::KwTry)) {
    return parse_try();
  }
  if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus) || check(TokenType::KwStaticCast) ||
      (check(TokenType::Identifier) &&
       (peek_at(1).type == TokenType::PlusPlus || peek_at(1).type == TokenType::MinusMinus))) {
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
  if (check(TokenType::KwGuard)) return parse_guard();
  if (check(TokenType::KwWhile)) {
    return parse_while();
  }
  if (check(TokenType::KwDo)) return parse_do_while();
  if (check(TokenType::KwMatch)) {
    return parse_match();
  }
  if (check(TokenType::KwReturn)) {
    return parse_return();
  }
  const std::size_t declaration_offset = typed_name_offset();
  if (declaration_offset != 0 && peek_at(declaration_offset).type == TokenType::Identifier &&
      peek_at(declaration_offset + 1).type == TokenType::Equal) {
    const std::string declared_type = parse_type_annotation();
    if (declared_type.empty()) {
      return std::nullopt;
    }
    return parse_initialized_name(declared_type);
  }
  if (check(TokenType::At) || check(TokenType::OpenParen) || check(TokenType::KwJoin) || check(TokenType::KwAwait) || check(TokenType::KwSpawn) ||
      (is_callable_name(peek().type) &&
       (starts_generic_call(1) || peek_at(1).type == TokenType::OpenBracket || peek_at(1).type == TokenType::OpenParen || peek_at(1).type == TokenType::Scope ||
        (peek_at(1).type == TokenType::Dot && peek_at(2).type == TokenType::Identifier &&
         peek_at(3).type == TokenType::OpenParen)))) {
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      synchronize();
      return std::nullopt;
    }
    const bool incomplete_member = check(TokenType::Colon);
    if (!consume(TokenType::Semicolon, "expected ';'") && !incomplete_member && !expr->missing && expr->kind != Expr::Kind::Member) {
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

std::optional<Stmt> Parser::parse_auto() {
  advance();
  std::optional<Stmt> stmt = parse_initialized_name({});
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = false;
  return stmt;
}

std::optional<Stmt> Parser::parse_observable() {
  advance();
  std::string type_name;
  if (check(TokenType::KwAuto)) {
    advance();
  } else {
    type_name = parse_type_annotation();
    if (type_name.empty() || !check(TokenType::Identifier)) {
      error("observable declarations require a type or auto before their name");
      synchronize(); return std::nullopt;
    }
  }
  std::optional<Stmt> stmt = parse_initialized_name(std::move(type_name));
  if (!stmt) {
    return std::nullopt;
  }
  stmt->immutable = false;
  stmt->observable_cell = true;
  return stmt;
}

struct FoldedNumber { double floating; std::int64_t integer; bool is_float; };

std::optional<FoldedNumber> fold_number(const Expr& expr, bool& division_by_zero, bool& overflow) {
  if (expr.kind == Expr::Kind::Number) {
    if (expr.bool_literal || expr.null_literal) return std::nullopt;
    const auto integer = expr.exact_integer ? std::optional<std::int64_t>{expr.integer} : numeric::from_float(expr.number);
    if (!expr.float_literal && !integer) { overflow = true; return std::nullopt; }
    return FoldedNumber{expr.number, integer.value_or(0), expr.float_literal};
  }
  if (!expr.left || !expr.right || (expr.kind != Expr::Kind::Add && expr.kind != Expr::Kind::Sub &&
      expr.kind != Expr::Kind::Mul && expr.kind != Expr::Kind::Div && expr.kind != Expr::Kind::Mod)) return std::nullopt;
  const auto left = fold_number(*expr.left, division_by_zero, overflow);
  const auto right = fold_number(*expr.right, division_by_zero, overflow);
  if (!left || !right || division_by_zero || overflow) return std::nullopt;
  if ((expr.kind == Expr::Kind::Div || expr.kind == Expr::Kind::Mod) && right->floating == 0) {
    division_by_zero = true; return std::nullopt;
  }
  if (!left->is_float && !right->is_float) {
    const auto a = left->integer, b = right->integer;
    const auto result = expr.kind == Expr::Kind::Add ? numeric::add(a, b) : expr.kind == Expr::Kind::Sub ? numeric::subtract(a, b) :
        expr.kind == Expr::Kind::Mul ? numeric::multiply(a, b) : expr.kind == Expr::Kind::Div ? numeric::divide(a, b) : numeric::remainder(a, b);
    if (!result) { overflow = true; return std::nullopt; }
    return FoldedNumber{static_cast<double>(*result), *result, false};
  }
  const auto a = left->floating, b = right->floating;
  const auto result = expr.kind == Expr::Kind::Add ? a + b : expr.kind == Expr::Kind::Sub ? a - b :
      expr.kind == Expr::Kind::Mul ? a * b : expr.kind == Expr::Kind::Div ? a / b : std::fmod(a, b);
  return FoldedNumber{result, 0, true};
}

std::optional<Stmt> Parser::parse_constexpr() {
  std::optional<Stmt> stmt = parse_const();
  if (!stmt) {
    return std::nullopt;
  }
  if (stmt->expr.kind == Expr::Kind::String ||
      (stmt->expr.kind == Expr::Kind::Number && (stmt->expr.bool_literal || stmt->expr.null_literal))) return stmt;
  bool division_by_zero = false;
  bool overflow = false;
  const auto folded = fold_number(stmt->expr, division_by_zero, overflow);
  if (overflow) { error("integer overflow"); return std::nullopt; }
  if (division_by_zero) {
    error("division by zero");
    return std::nullopt;
  }
  if (!folded) {
    error("not a constant");
    return std::nullopt;
  }
  const auto location = stmt->expr.location;
  const auto span = stmt->expr.span;
  stmt->expr = Expr{};
  stmt->expr.location = location; stmt->expr.span = span;
  stmt->expr.kind = Expr::Kind::Number;
  stmt->expr.number = folded->floating;
  stmt->expr.integer = folded->integer;
  stmt->expr.exact_integer = !folded->is_float;
  stmt->expr.float_literal = folded->is_float;
  stmt->immutable = true;
  return stmt;
}

std::optional<Stmt> Parser::parse_signal() {
  if (peek_at(1).type == TokenType::Less) {
    const auto type = parse_type_annotation();
    if (!check(TokenType::Identifier)) { error("expected name"); return std::nullopt; }
    const auto name = peek(); advance();
    Stmt stmt; stmt.kind = Stmt::Kind::VariableDecl; stmt.declared_type = type;
    stmt.name = std::string(name.lexeme); stmt.name_span = name.lexeme; stmt.name_location = name.location;
    if (check(TokenType::Equal)) {
      advance(); auto initializer = parse_expression(); if (!initializer) return std::nullopt; stmt.expr = std::move(*initializer);
    } else {
      stmt.expr.kind = Expr::Kind::Event; stmt.expr.runtime_op = 0; stmt.expr.value = type;
      stmt.expr.span = name.lexeme; stmt.expr.location = name.location;
    }
    if (!consume(TokenType::Semicolon, "expected ';'")) return std::nullopt;
    return stmt;
  }
  error("event declarations require signal<PayloadTypes>, including signal<>");
  synchronize(); return std::nullopt;
}

std::optional<Stmt> Parser::parse_connect() {
  if (peek_at(2).type != TokenType::Identifier || peek_at(2).lexeme != "Connect" || peek_at(3).type != TokenType::OpenParen) {
    error("managed event subscriptions use event~>Connect(callback)"); synchronize(); return std::nullopt;
  }
  auto value = parse_expression();
  if (!value || !consume(TokenType::Semicolon, "expected ';'")) return std::nullopt;
  Stmt stmt; stmt.kind = Stmt::Kind::Expr; stmt.expr = std::move(*value); return stmt;
}

std::optional<Stmt> Parser::parse_const() {
  advance();
  std::string declared_type;
  if (check(TokenType::KwAuto)) {
    advance();
  } else {
    declared_type = parse_type_annotation();
    if (declared_type.empty()) {
      synchronize();
      return std::nullopt;
    }
  }
  std::optional<Stmt> stmt = parse_initialized_name(std::move(declared_type));
  if (stmt) stmt->immutable = true;
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
    return is_type_name(type) || type == TokenType::KwAuto || type == TokenType::KwFunc || type == TokenType::KwPost ||
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
  stmt.kind = Stmt::Kind::VariableDecl;
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
  if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus) ||
      (check(TokenType::Identifier) && (peek_at(1).type == TokenType::PlusPlus || peek_at(1).type == TokenType::MinusMinus))) {
    auto expression = parse_expression();
    if (!expression || (require_semicolon && !consume(TokenType::Semicolon, "expected ';'"))) return std::nullopt;
    Stmt statement; statement.kind = Stmt::Kind::Expr; statement.expr = std::move(*expression);
    return statement;
  }
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

// `name(.field | [index])+ (= | += | -= | *= | /= | %=)` â€” decided by lookahead only, so
// expression statements such as `p.move(1);` or `xs[0];` are untouched.
bool Parser::at_place_assign() const {
  const bool receiver = check(TokenType::At) && peek_at(1).type == TokenType::Identifier && peek_at(1).lexeme == "this";
  if (!check(TokenType::Identifier) && !receiver) {
    return false;
  }
  std::size_t at = receiver ? 2 : 1;
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
  const bool receiver = check(TokenType::At);
  if (receiver) advance();
  const Token root = peek();
  advance();
  Expr place;
  place.kind = Expr::Kind::Name;
  place.value = receiver ? "@this" : std::string(root.lexeme);
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
  stmt.name = root.type == TokenType::At ? "@this" : std::string(root.lexeme);
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

std::optional<Stmt> Parser::parse_guard() {
  const Token mark = peek();
  advance();
  if (!consume(TokenType::OpenParen, "expected '('")) return std::nullopt;
  auto condition = parse_expression();
  if (!condition || !consume(TokenType::CloseParen, "expected ')'")) return std::nullopt;
  if (!consume(TokenType::KwElse, "expected else")) return std::nullopt;
  auto body = parse_block();
  if (!body) return std::nullopt;
  Stmt stmt;
  stmt.kind = Stmt::Kind::If;
  stmt.is_guard = true;
  stmt.expr.kind = Expr::Kind::Not;
  stmt.expr.location = mark.location;
  stmt.expr.span = mark.lexeme;
  stmt.expr.left = std::make_unique<Expr>(std::move(*condition));
  stmt.then_body = std::move(*body);
  return stmt;
}

std::optional<Stmt> Parser::parse_if() {
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

std::optional<Stmt> Parser::parse_do_while() {
  advance();
  auto body = parse_block();
  if (!body || !consume(TokenType::KwWhile, "expected while after do block") ||
      !consume(TokenType::OpenParen, "expected '('")) return std::nullopt;
  auto condition = parse_expression();
  if (!condition || !consume(TokenType::CloseParen, "expected ')'" ) ||
      !consume(TokenType::Semicolon, "expected ';' after do/while")) return std::nullopt;
  Stmt statement; statement.kind = Stmt::Kind::While; statement.post_condition = true;
  statement.then_body = std::move(*body); statement.expr = std::move(*condition);
  return statement;
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

std::optional<Stmt> Parser::parse_for_in(std::string declared_type) {
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
  const AnnotationTypeScope value_scope(m_expression_names, {std::string(name.lexeme)});
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  Stmt stmt;
  stmt.kind = Stmt::Kind::ForIn;
  stmt.declared_type = std::move(declared_type);
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
  std::optional<Stmt> init;
  const std::size_t header_start = m_current;
  if (check(TokenType::KwAuto) || is_type_name(peek().type) || check(TokenType::Identifier)) {
    std::string declared_type;
    if (check(TokenType::KwAuto)) {
      advance();
    } else {
      declared_type = parse_type_annotation();
    }
    if (check(TokenType::Identifier) && peek_at(1).type == TokenType::KwIn) {
      return parse_for_in(std::move(declared_type));
    }
    if (check(TokenType::Identifier) && peek_at(1).type == TokenType::Equal) {
      init = parse_initialized_name(std::move(declared_type));
    } else {
      m_current = header_start;
      init = parse_update(true);
    }
  } else if (check(TokenType::KwConst)) {
    init = parse_const();
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
  const AnnotationTypeScope value_scope(m_expression_names, {init->name});
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
                          (peek_at(1).type == TokenType::FatArrow || peek_at(1).type == TokenType::KwGuard);
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
    if (!consume(TokenType::FatArrow, "match arms require =>")) {
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

std::string Parser::parse_generic_suffix() {
  if (!check(TokenType::Less)) {
    return {};
  }
  advance();
  std::string text = "<";
  if (check(TokenType::Greater)) { advance(); return "<>"; }
  while (!is_at_end()) {
    const std::string arg = parse_type_annotation();
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

bool Parser::starts_generic_call(const std::size_t offset) const {
  if (peek_at(offset).type != TokenType::Less) {
    return false;
  }
  int depth = 0;
  for (std::size_t index = offset;; ++index) {
    const TokenType type = peek_at(index).type;
    if (type == TokenType::Eof) {
      return false;
    }
    if (type == TokenType::Less) {
      ++depth;
    } else if (type == TokenType::Greater) {
      --depth;
      if (depth == 0) {
        return peek_at(index + 1).type == TokenType::OpenParen || peek_at(index + 1).type == TokenType::OpenBrace;
      }
    }
  }
}

bool Parser::starts_generic_value(const std::size_t offset) const {
  if (peek_at(offset).type != TokenType::Less) return false;
  int depth = 0;
  for (std::size_t index = offset;; ++index) {
    const auto type = peek_at(index).type;
    if (type == TokenType::Eof || type == TokenType::Semicolon ||
        type == TokenType::Equal || type == TokenType::OpenBrace || type == TokenType::CloseBrace) return false;
    if (type == TokenType::Less) ++depth;
    else if (type == TokenType::Greater && --depth == 0) {
      // Only expression boundaries disambiguate a value from a comparison.
      const auto next = peek_at(index + 1).type;
      return next == TokenType::Semicolon || next == TokenType::Comma ||
             next == TokenType::CloseParen || next == TokenType::CloseBracket ||
             next == TokenType::CloseBrace || next == TokenType::Eof;
    }
  }
}

std::string Parser::parse_type_atom() {
  if (check(TokenType::OpenParen)) {
    advance();
    std::string text = "(";
    if (!check(TokenType::CloseParen)) {
      do {
        if (text.size() > 1) { text += ','; advance(); }
        const auto part = parse_type_annotation();
        if (part.empty()) return {};
        text += part;
      } while (check(TokenType::Comma));
    }
    if (!consume(TokenType::CloseParen, "expected ')' in callback type") ||
        !consume(TokenType::Arrow, "expected '->' in callback type")) return {};
    const auto result = parse_type_annotation();
    return result.empty() ? std::string{} : text + ")->" + result;
  }
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
    // Preserve qualification until the importer resolves the module interface.
    while (check(TokenType::Dot) && peek_at(1).type == TokenType::Identifier) {
      advance();
      text += ".";
      text += std::string(peek().lexeme);
      advance();
    }
    text += parse_generic_suffix();
    return text;
  }
  error("expected type");
  return {};
}

std::string Parser::parse_type_annotation() {
  if (!nesting_available(m_type_depth)) return {};
  const SyntaxDepth type_depth(m_type_depth);
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
  if (!starts_typed_function()) {
    error("extern declarations use an explicit return type before the function name");
    synchronize();
    return std::nullopt;
  }
  auto function = parse_typed_function(true);
  if (!function) return std::nullopt;
  if (function->name != "strlen") {
    error("unknown extern");
    return std::nullopt;
  }
  if (function->return_type != "number" || function->param_types != std::vector<std::string>{"string"} ||
      function->variadic || !function->body.empty()) {
    error("extern strlen requires signature: extern number strlen(string value);");
    return std::nullopt;
  }
  function->is_extern = true;
  function->extern_id = 0;
  return function;
}

std::size_t Parser::typed_name_offset() const {
  if (!is_type_name(peek().type) && !check(TokenType::Identifier) && !check(TokenType::KwVoid)) return 0;
  std::size_t offset = 1;
  while (peek_at(offset).type == TokenType::Dot && peek_at(offset + 1).type == TokenType::Identifier) offset += 2;
  if (peek_at(offset).type == TokenType::Less) {
    int depth = 0;
    do {
      const TokenType type = peek_at(offset++).type;
      if (type == TokenType::Eof || type == TokenType::Semicolon || type == TokenType::OpenBrace) return 0;
      if (type == TokenType::Less) ++depth;
      if (type == TokenType::Greater) --depth;
    } while (depth > 0);
  }
  return offset;
}

bool Parser::starts_typed_function() const {
  const std::size_t offset = typed_name_offset();
  if (offset != 0 && peek_at(offset).type == TokenType::KwOperator) {
    const TokenType mark = peek_at(offset + 1).type;
    return ((mark == TokenType::Plus || mark == TokenType::Minus) && peek_at(offset + 2).type == TokenType::OpenParen) ||
        (mark == TokenType::OpenBracket && peek_at(offset + 2).type == TokenType::CloseBracket && peek_at(offset + 3).type == TokenType::OpenParen);
  }
  return offset != 0 && peek_at(offset).type == TokenType::Identifier &&
      (peek_at(offset + 1).type == TokenType::OpenParen ||
       (peek_at(offset + 1).type == TokenType::Scope && peek_at(offset + 2).type == TokenType::Identifier &&
        peek_at(offset + 3).type == TokenType::OpenParen));
}

std::optional<Function> Parser::parse_generic_function(const bool allow_signature) {
  advance();  // template
  if (!consume(TokenType::Less, "expected '<'")) return std::nullopt;
  std::vector<std::string> parameters, constraint_names, constraints;
  do {
    if (!consume(TokenType::KwTypename, "expected 'typename'")) return std::nullopt;
    if (!check(TokenType::Identifier)) { error("expected type parameter"); return std::nullopt; }
    const std::string parameter(peek().lexeme);
    if (std::find(parameters.begin(), parameters.end(), parameter) != parameters.end()) {
      error("duplicate type parameter"); return std::nullopt;
    }
    parameters.push_back(parameter);
    advance();
    if (check(TokenType::Colon)) {
      advance();
      constraint_names.push_back(parameter);
      constraints.push_back(parse_type_annotation());
      if (constraints.back().empty()) return std::nullopt;
    }
    if (!check(TokenType::Comma)) break;
    advance();
  } while (!is_at_end());
  if (!consume(TokenType::Greater, "expected '>'")) return std::nullopt;
  const bool asynchronous = check(TokenType::KwAsync);
  if (asynchronous) advance();
  if (!starts_typed_function()) { error("expected typed function"); return std::nullopt; }
  const AnnotationTypeScope annotation_scope(m_annotation_types, parameters);
  auto function = parse_typed_function(allow_signature);
  if (function) {
    for (const auto& parameter : parameters) {
      if (std::find(function->type_params.begin(), function->type_params.end(), parameter) != function->type_params.end()) {
        error("method type parameter conflicts with data type parameter"); return std::nullopt;
      }
    }
    function->type_params.insert(function->type_params.end(), parameters.begin(), parameters.end());
    constraint_names.insert(constraint_names.end(), function->where_names.begin(), function->where_names.end());
    constraints.insert(constraints.end(), function->where_types.begin(), function->where_types.end());
    function->where_names = std::move(constraint_names);
    function->where_types = std::move(constraints);
    function->is_async = asynchronous;
  }
  return function;
}

std::optional<Function> Parser::parse_typed_function(const bool allow_signature) {
  const std::string return_type = parse_type_annotation();
  if (!check(TokenType::Identifier) && !check(TokenType::KwOperator)) {
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
  function.return_type = return_type;
  if (name.type == TokenType::KwOperator) {
    if (check(TokenType::Plus) || check(TokenType::Minus)) {
      function.name += std::string(peek().lexeme);
      advance();
    } else if (check(TokenType::OpenBracket)) {
      advance();
      if (!consume(TokenType::CloseBracket, "expected ']'")) return std::nullopt;
      function.name += "[]";
    } else {
      error("unsupported operator declaration"); return std::nullopt;
    }
  }
  const StructDecl* owner = nullptr;
  if (check(TokenType::Scope)) {
    advance();
    if (!check(TokenType::Identifier)) { error("expected method name"); return std::nullopt; }
    if (m_program) for (const auto& data : m_program->structs) if (data.name == function.name) { owner = &data; break; }
    if (!owner) { error("external method requires a declared local data type"); return std::nullopt; }
    function.name += "." + std::string(peek().lexeme);
    function.name_span = peek().lexeme; function.name_location = peek().location;
    function.owner_type_param_count = owner->type_params.size(); function.type_params = owner->type_params;
    advance();
  }
  if (!parse_parameter_list(function)) {
    return std::nullopt;
  }
  if (std::any_of(function.param_types.begin(), function.param_types.end(),
                  [](const std::string& type) { return type.empty(); })) {
    error("named function parameters require explicit types");
    return std::nullopt;
  }
  if (owner) {
    std::string receiver = owner->name;
    if (!owner->type_params.empty()) {
      receiver += '<';
      for (std::size_t index = 0; index < owner->type_params.size(); ++index) {
        if (index) receiver += ',';
        receiver += owner->type_params[index];
      }
      receiver += '>';
    }
    function.params.insert(function.params.begin(), "@this");
    function.param_types.insert(function.param_types.begin(), std::move(receiver));
    function.param_spans.insert(function.param_spans.begin(), owner->name_span);
    function.param_locations.insert(function.param_locations.begin(), owner->name_location);
  }
  if (!parse_where_clause(function)) return std::nullopt;
  if (allow_signature && check(TokenType::Semicolon)) {
    advance();
    return function;
  }
  const AnnotationTypeScope formal_scope(m_annotation_types, function.type_params);
  const AnnotationTypeScope value_scope(m_expression_names, function.params);
  std::optional<std::vector<Stmt>> body = parse_block();
  if (!body) {
    return std::nullopt;
  }
  function.body = std::move(*body);
  return function;
}

bool Parser::parse_where_clause(Function& function) {
  if (!check(TokenType::KwWhere)) return true;
  advance();
  do {
    if (!check(TokenType::Identifier)) {
      error("expected type parameter after 'where' or ','");
      return false;
    }
    function.where_names.emplace_back(peek().lexeme);
    advance();
    if (!consume(TokenType::Colon, "expected ':'")) return false;
    auto constraint = parse_type_annotation();
    if (constraint.empty()) return false;
    function.where_types.push_back(std::move(constraint));
    if (!check(TokenType::Comma)) break;
    advance();
  } while (!is_at_end());
  return true;
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
  if (!std::string_view(path.lexeme).ends_with(".clp")) {
    m_diagnostics.push_back(Diagnostic{path.location, path.lexeme, "source modules must use .clp; CL++ has no headers"});
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
  if (!consume(TokenType::KwDef, "data declarations use def")) return std::nullopt;
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
    do {
      const Token base = peek();
      const auto spelling = parse_type_annotation();
      if (spelling.empty()) return std::nullopt;
      if (decl.base.empty()) {
        decl.base = spelling; decl.base_span = base.lexeme; decl.base_location = base.location;
      }
      decl.bases.push_back(spelling); decl.base_spans.push_back(base.lexeme); decl.base_locations.push_back(base.location);
      if (!check(TokenType::Comma)) break;
      advance();
    } while (!is_at_end());
  }
  if (!consume(TokenType::OpenBrace, "expected '{'")) {
    synchronize();
    return std::nullopt;
  }
  auto annotation_types = decl.type_params;
  annotation_types.push_back(decl.name);
  const AnnotationTypeScope annotation_scope(m_annotation_types, annotation_types);
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    bool is_private = false;
    bool is_override = false;
    bool is_abstract_method = false;
    bool is_final_method = false;
    bool is_async_method = false;
    while (check(TokenType::KwPrivate) || check(TokenType::KwPublic) || check(TokenType::KwOverride) ||
           check(TokenType::KwAbstract) || check(TokenType::KwFinal) || check(TokenType::KwAsync)) {
      if (check(TokenType::KwPrivate)) {
        is_private = true;
      } else if (check(TokenType::KwPublic)) {
        is_private = false;
      } else if (check(TokenType::KwOverride)) {
        is_override = true;
      } else if (check(TokenType::KwAsync)) {
        is_async_method = true;
      } else if (check(TokenType::KwFinal)) {
        is_final_method = true;
      } else {
        is_abstract_method = true;
      }
      advance();
    }
    if (check(TokenType::KwFunc)) {
      error("named methods use return-type-first declarations; func is only for lambdas");
      synchronize();
      return std::nullopt;
    }
    if (check(TokenType::KwTemplate) || starts_typed_function()) {
      const bool generic_method = check(TokenType::KwTemplate);
      std::optional<Function> method = generic_method ? parse_generic_function(is_abstract_method) :
          parse_typed_function(is_abstract_method);
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
      method->is_async = method->is_async || is_async_method;
      if (!generic_method && !method->type_params.empty()) {
        error("generic methods require template declarations");
        return std::nullopt;
      }
      for (const auto& parameter : method->type_params) {
        if (std::find(decl.type_params.begin(), decl.type_params.end(), parameter) != decl.type_params.end()) {
          error("method type parameter conflicts with data type parameter");
          return std::nullopt;
        }
      }
      method->owner_type_param_count = decl.type_params.size();
      method->type_params.insert(method->type_params.begin(), decl.type_params.begin(), decl.type_params.end());
      std::string receiver_type = decl.name;
      if (!decl.type_params.empty()) {
        receiver_type += '<';
        for (std::size_t index = 0; index < decl.type_params.size(); ++index) {
          if (index) receiver_type += ',';
          receiver_type += decl.type_params[index];
        }
        receiver_type += '>';
      }
      method->params.insert(method->params.begin(), "@this");
      method->param_types.insert(method->param_types.begin(), std::move(receiver_type));
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
    const std::string type_name = parse_type_annotation();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      synchronize();
      return std::nullopt;
    }
    const Token field_name = peek();
    advance();
    std::shared_ptr<Expr> default_value;
    if (check(TokenType::Equal)) {
      advance();
      auto value = parse_expression();
      if (!value) return std::nullopt;
      default_value = std::make_shared<Expr>(std::move(*value));
    }
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
      field.default_value = std::move(default_value);
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
  if (!consume(TokenType::KwClass, "enum declarations require enum class")) { synchronize(); return std::nullopt; }
  if (!check(TokenType::Identifier)) {
    error("expected name");
    synchronize();
    return std::nullopt;
  }
  const Token name = peek();
  advance();
  EnumDecl decl;
  decl.scoped = true;
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
      std::size_t qualified_end = 0;
      if (check(TokenType::Identifier)) {
        qualified_end = 1;
        while (peek_at(qualified_end).type == TokenType::Dot && peek_at(qualified_end + 1).type == TokenType::Identifier)
          qualified_end += 2;
      }
      // number x, Fighter f, Combat.Fighter f, array<number> xs, any ...values
      if (is_type_name(peek().type) ||
          (check(TokenType::Identifier) &&
           (peek_at(qualified_end).type == TokenType::Identifier || peek_at(qualified_end).type == TokenType::Less ||
            peek_at(qualified_end).type == TokenType::Ellipsis))) {
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
      advance();
      if (check(TokenType::Equal)) {
        error("function parameters cannot have default values; use def field defaults");
        synchronize();
        return false;
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

  const AnnotationTypeScope value_scope(m_expression_names, {});
  ++m_block_depth;
  std::vector<Stmt> body;
  while (!check(TokenType::CloseBrace) && !is_at_end()) {
    if (check(TokenType::Invalid)) {
      advance();
      continue;
    }
    const std::size_t before = m_current;
    if (std::optional<Stmt> stmt = parse_statement()) {
      if (stmt->kind == Stmt::Kind::VariableDecl || stmt->kind == Stmt::Kind::Signal) m_expression_names.push_back(stmt->name);
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
  if (!nesting_available(m_expression_depth)) return std::nullopt;
  const SyntaxDepth expression_depth(m_expression_depth);
  std::optional<Expr> expr = parse_or();
  if (!expr) {
    return std::nullopt;
  }
  if (!check(TokenType::Question)) {
    if (!expression_depth_valid(*expr)) return std::nullopt;
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
  if (!expression_depth_valid(ternary)) return std::nullopt;
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
  std::optional<Expr> expr = parse_comparison();
  if (!expr) {
    return std::nullopt;
  }
  while (check(TokenType::KwShl) || check(TokenType::KwShr)) {
    const Expr::Kind kind = check(TokenType::KwShl) ? Expr::Kind::Shl : Expr::Kind::Shr;
    advance();
    std::optional<Expr> right = parse_comparison();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }
  return expr;
}

std::optional<Expr> Parser::parse_unary() {
  if (!nesting_available(m_unary_depth)) return std::nullopt;
  const SyntaxDepth unary_depth(m_unary_depth);
  if (check(TokenType::KwStaticCast)) {
    const Token token = peek(); advance();
    if (!consume(TokenType::Less, "expected '<'")) return std::nullopt;
    const auto target = parse_type_annotation();
    if (target.empty() || !consume(TokenType::Greater, "expected '>'") ||
        !consume(TokenType::OpenParen, "expected '('")) return std::nullopt;
    auto operand = parse_expression();
    if (!operand || !consume(TokenType::CloseParen, "expected ')'")) return std::nullopt;
    Expr cast;
    cast.kind = Expr::Kind::Cast; cast.value = target; cast.location = token.location; cast.span = token.lexeme;
    cast.left = std::make_unique<Expr>(std::move(*operand));
    return finish_postfix(std::move(cast));
  }
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
    if (check(TokenType::IntLiteral)) {
      const auto value = numeric::parse_integer(peek().lexeme, true);
      if (!value) { error("integer literal out of range"); return std::nullopt; }
      Expr literal;
      literal.kind = Expr::Kind::Number; literal.integer = *value; literal.exact_integer = true;
      literal.number = static_cast<double>(*value); literal.location = token.location; literal.span = peek().lexeme;
      advance();
      return finish_postfix(std::move(literal));
    }
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
    return parse_primary();
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
  std::optional<Expr> expr = parse_unary();
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
    std::optional<Expr> right = parse_unary();
    if (!right) {
      return std::nullopt;
    }
    expr = make_binary(kind, std::move(*expr), std::move(*right));
  }

  return expr;
}

std::optional<Expr> Parser::parse_self_field() {
  advance();
  if (!check(TokenType::Identifier) || peek().lexeme != "this") {
    error("instance access uses @this.Field"); return std::nullopt;
  }
  const Token name = peek(); advance();
  if (check(TokenType::Scope)) {
    error("instance access uses @this.Field, not @this::Field"); return std::nullopt;
  }
  Expr receiver; receiver.kind = Expr::Kind::Name; receiver.value = "@this";
  receiver.span = name.lexeme; receiver.location = name.location;
  return receiver;
}

std::optional<Expr> Parser::parse_primary() {
  if (check(TokenType::OpenBrace)) {
    const Token mark = peek(); advance();
    Expr values; values.kind = Expr::Kind::MakeList; values.value = "$array_initializer";
    values.location = mark.location; values.span = mark.lexeme;
    while (!check(TokenType::CloseBrace) && !is_at_end()) {
      auto value = parse_expression();
      if (!value) return std::nullopt;
      values.args.push_back(std::move(*value));
      if (!check(TokenType::Comma)) break;
      advance();
    }
    if (!consume(TokenType::CloseBrace, "expected '}'")) return std::nullopt;
    return finish_postfix(std::move(values));
  }
  if (((check(TokenType::KwFunc) && peek_at(1).type == TokenType::OpenParen) ||
       (check(TokenType::KwAsync) && peek_at(1).type == TokenType::KwFunc && peek_at(2).type == TokenType::OpenParen)) && m_program != nullptr) {
    const bool asynchronous = check(TokenType::KwAsync);
    if (asynchronous) advance();
    const Token mark = peek();
    advance();
    Function function;
    function.anonymous = true;
    function.is_async = asynchronous;
    function.name = "$lambda_" + std::to_string(mark.location.line) + "_" + std::to_string(mark.location.column);
    function.name_span = mark.lexeme;
    function.name_location = mark.location;
    if (!parse_parameter_list(function)) {
      return std::nullopt;
    }
    if (!consume(TokenType::Arrow, "lambda requires an explicit result type after ->")) return std::nullopt;
    function.return_type = parse_type_annotation();
    const AnnotationTypeScope value_scope(m_expression_names, function.params);
    std::optional<std::vector<Stmt>> body = parse_block();
    if (!body) return std::nullopt;
    function.body = std::move(*body);
    Expr named;
    named.kind = Expr::Kind::Closure;
    named.value = function.name;
    named.span = mark.lexeme;
    named.location = mark.location;
    m_program->functions.push_back(std::move(function));
    return finish_postfix(std::move(named));
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
    return finish_postfix(std::move(expr));
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
    auto receiver = parse_self_field();
    if (!receiver) return std::nullopt;
    return finish_postfix(std::move(*receiver));
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
    if (!expr.float_literal) {
      const auto integer = numeric::parse_integer(peek().lexeme);
      if (!integer) { error("integer literal out of range"); return std::nullopt; }
      expr.integer = *integer; expr.exact_integer = true;
    }
    expr.location = peek().location;
    expr.span = peek().lexeme;
    advance();
    return finish_postfix(std::move(expr));
  }

  if (is_callable_name(peek().type) && peek_at(1).type == TokenType::FatArrow) {
    error("shorthand lambdas are not supported; use func(parameters) -> Type { body }");
    return std::nullopt;
  }
  if (is_callable_name(peek().type)) {
    const Token name = peek();
    advance();
    std::string callable(name.lexeme);
    if (starts_generic_call() || starts_generic_value()) {
      callable += parse_generic_suffix();
    }
    if (check(TokenType::Scope)) {
      advance();
      if (!check(TokenType::Identifier)) {
        error("expected name");
        return std::nullopt;
      }
      Expr owner;
      owner.kind = Expr::Kind::Name;
      owner.value = callable;
      owner.location = name.location;
      owner.span = name.lexeme;
      Expr member;
      member.kind = Expr::Kind::Member;
      member.scoped_member = true;
      member.value = std::string(peek().lexeme);
      member.location = peek().location;
      member.span = peek().lexeme;
      member.left = std::make_unique<Expr>(std::move(owner));
      advance();
      if (!check(TokenType::OpenParen)) return finish_postfix(std::move(member));
      // Static library operations are calls; enum cases are values.
      callable += "::" + member.value;
    }
    if (check(TokenType::Dot) && peek_at(1).type == TokenType::Identifier &&
        (peek_at(2).type == TokenType::OpenParen || peek_at(2).type == TokenType::OpenBrace || starts_generic_call(2) || starts_generic_value(2))) {
      advance();
      callable.push_back('.');
      callable.append(peek().lexeme);
      advance();
      if (starts_generic_call() || starts_generic_value()) callable += parse_generic_suffix();
    }
    if (check(TokenType::OpenBrace)) {
      advance();
      Expr call;
      call.kind = Expr::Kind::Call;
      call.named_initializer = true;
      call.value = std::move(callable);
      call.span = name.lexeme;
      call.location = name.location;
      while (!check(TokenType::CloseBrace) && !is_at_end()) {
        if (!check(TokenType::Identifier)) {
          error("expected field name");
          return std::nullopt;
        }
        call.arg_names.emplace_back(peek().lexeme);
        advance();
        if (!consume(TokenType::Equal, "expected '='")) return std::nullopt;
        auto value = parse_expression();
        if (!value) return std::nullopt;
        call.args.push_back(std::move(*value));
        if (!check(TokenType::Comma)) break;
        advance();
      }
      if (!consume(TokenType::CloseBrace, "expected '}'")) return std::nullopt;
      return finish_postfix(std::move(call));
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
        if (check(TokenType::Identifier) && peek_at(1).type == TokenType::Colon) {
          error("named call arguments are not supported; use positional arguments or def field initializers");
          return std::nullopt;
        }
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
    return finish_postfix(std::move(call));
  }

  if (check(TokenType::OpenParen)) {
    advance();
    const std::size_t start = m_current;
    const std::size_t diagnostics_start = m_diagnostics.size();
    const Token type_token = peek();
    const std::string type = parse_type_annotation();
    const std::string type_root = type.substr(0, type.find('<'));
    const auto next = peek().type;
    const bool prefix_candidate = next == TokenType::OpenParen || next == TokenType::Minus ||
        next == TokenType::Bang || next == TokenType::KwNot || next == TokenType::BitNot;
    bool known_type = is_type_name(type_token.type) || type_token.type == TokenType::KwVoid;
    bool array_type = type_root == "array";
    if (prefix_candidate && !known_type) {
      known_type = std::find(m_annotation_types.begin(), m_annotation_types.end(), type_root) != m_annotation_types.end();
      if (m_program) {
        for (const auto& alias : m_program->aliases) known_type = known_type || alias.name == type_root;
        for (const auto& data : m_program->structs) known_type = known_type || data.name == type_root;
        for (const auto& enumeration : m_program->enums) known_type = known_type || enumeration.name == type_root;
      }
    }
    if (next == TokenType::OpenBrace && !array_type && m_program) {
      for (const auto& alias : m_program->aliases) {
        array_type = array_type || (alias.name == type_root && alias.body.starts_with("array<"));
      }
    }
    bool value_name = false;
    if (prefix_candidate && known_type && type_token.type == TokenType::Identifier) {
      value_name = std::find(m_expression_names.begin(), m_expression_names.end(), type_root) != m_expression_names.end();
      if (m_program) for (const auto& statement : m_program->statements) {
        value_name = value_name || statement.name == type_root;
      }
    }
    const bool array_braces = next == TokenType::OpenBrace && (array_type ||
        (peek_at(1).type != TokenType::CloseBrace &&
         !(peek_at(1).type == TokenType::Identifier && peek_at(2).type == TokenType::Equal)));
    const bool adjacent_value = next == TokenType::Identifier || next == TokenType::StringLiteral ||
        next == TokenType::IntLiteral || next == TokenType::FloatLiteral ||
        next == TokenType::TemplateString || next == TokenType::TemplateHead ||
        next == TokenType::KwTrue || next == TokenType::KwFalse || next == TokenType::KwNull ||
        next == TokenType::KwFunc || next == TokenType::KwAsync || next == TokenType::At ||
        next == TokenType::KwAwait || next == TokenType::KwSpawn || next == TokenType::KwThread ||
        array_braces || is_type_name(next);
    const bool separated = prefix_candidate && m_current > start && !peek().lexeme.empty() && peek().lexeme.data() >
        m_tokens[m_current - 1].lexeme.data() + m_tokens[m_current - 1].lexeme.size();
    const bool known_prefix = known_type && !value_name && separated && (next == TokenType::OpenParen || next == TokenType::Minus ||
        next == TokenType::Bang || next == TokenType::KwNot || next == TokenType::BitNot);
    if (!type.empty() && m_diagnostics.size() == diagnostics_start && (adjacent_value || known_prefix)) {
      auto operand = parse_expression();
      if (!operand || !consume(TokenType::CloseParen, "expected ')' after typed expression")) return std::nullopt;
      Expr typed;
      typed.kind = Expr::Kind::Ascribe; typed.value = type;
      typed.location = type_token.location; typed.span = type_token.lexeme;
      typed.left = std::make_unique<Expr>(std::move(*operand));
      return finish_postfix(std::move(typed));
    }
    m_current = start;
    m_diagnostics.resize(diagnostics_start);
    std::optional<Expr> expr = parse_expression();
    if (!expr) {
      return std::nullopt;
    }
    if (!consume(TokenType::CloseParen, "expected ')'")) {
      return std::nullopt;
    }
    if (expr->kind == Expr::Kind::Call && !type.empty() && expr->value == type &&
        ((next == TokenType::OpenParen && separated && expr->args.size() == 1) ||
         (next == TokenType::OpenBrace && expr->named_initializer && expr->args.empty()))) {
      expr->scoped_type_candidate = true;
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
        hole_token.location.source = token.location.source;
        hole_token.location.line = token.location.line;
        hole_token.location.column += hole_column - 1;
      }
      for (Diagnostic& diagnostic : lexed.diagnostics) {
        diagnostic.location.source = token.location.source;
        diagnostic.location.line = token.location.line;
        diagnostic.location.column += hole_column - 1;
      }
      Parser embedded(lexed.tokens, lexed.diagnostics);
      embedded.m_annotation_types = m_annotation_types;
      embedded.m_expression_names = m_expression_names;
      if (m_program) {
        for (const auto& alias : m_program->aliases) embedded.m_annotation_types.push_back(alias.name);
        for (const auto& data : m_program->structs) embedded.m_annotation_types.push_back(data.name);
      }
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
  if (!expression_depth_valid(expr)) return std::nullopt;
  while ((check(TokenType::Colon) && peek_at(1).type == TokenType::Identifier && peek_at(1).lexeme == "is") ||
         check(TokenType::Dot) || check(TokenType::Scope) || check(TokenType::OpenBracket) || check(TokenType::PlusPlus) ||
         check(TokenType::MinusMinus) || check(TokenType::OpenParen) || starts_generic_call() ||
         (expr.kind == Expr::Kind::Member && starts_generic_value()) ||
         (check(TokenType::SignalConnect) && peek_at(1).type == TokenType::Identifier &&
          peek_at(1).lexeme == "Connect" && peek_at(2).type == TokenType::OpenParen)) {
    if (!expression_depth_valid(expr)) return std::nullopt;
    if (check(TokenType::Colon)) {
      const Token token = peek(); advance(); advance();
      if (!consume(TokenType::Less, "expected '<' after :is")) return std::nullopt;
      const auto target = parse_type_annotation();
      if (target.empty() || !consume(TokenType::Greater, "expected '>' after tested type")) return std::nullopt;
      Expr test; test.kind = Expr::Kind::TypeTest; test.value = target;
      test.location = token.location; test.span = token.lexeme;
      test.left = std::make_unique<Expr>(std::move(expr));
      expr = std::move(test); continue;
    }
    if (check(TokenType::SignalConnect)) {
      const auto mark = peek(); advance();
      if (!check(TokenType::Identifier) || peek().lexeme != "Connect") { error("expected Connect after ~>"); return std::nullopt; }
      advance(); if (!consume(TokenType::OpenParen, "expected '('")) return std::nullopt;
      Expr call; call.kind = Expr::Kind::Call; call.value = "$ManagedConnect"; call.location = mark.location; call.span = mark.lexeme;
      call.left = std::make_unique<Expr>(std::move(expr));
      if (!check(TokenType::CloseParen)) do {
        auto argument = parse_expression(); if (!argument) return std::nullopt; call.args.push_back(std::move(*argument));
        if (!check(TokenType::Comma)) break;
        advance();
      } while (!is_at_end());
      if (!consume(TokenType::CloseParen, "expected ')'")) return std::nullopt;
      expr = std::move(call); continue;
    }
    if (expr.kind == Expr::Kind::Member && starts_generic_value() && !starts_generic_call()) {
      expr.value += parse_generic_suffix(); continue;
    }
    if (check(TokenType::OpenParen) || starts_generic_call()) {
      if (starts_generic_call()) expr.value += parse_generic_suffix();
      if (!consume(TokenType::OpenParen, "expected '('")) return std::nullopt;
      Expr call;
      call.kind = Expr::Kind::Call;
      call.value = expr.value;
      call.span = expr.span;
      call.location = expr.location;
      if (expr.kind == Expr::Kind::Member) call.left = std::move(expr.left);
      else if (expr.kind != Expr::Kind::Name) {
        call.kind = Expr::Kind::Invoke;
        call.left = std::make_unique<Expr>(std::move(expr));
      }
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
    const bool scoped = check(TokenType::Scope);
    advance();
    if (!check(TokenType::Identifier)) {
      error("expected name");
      Expr member;
      member.kind = Expr::Kind::Member;
      member.missing = true;
      member.scoped_member = scoped;
      member.location = peek().location;
      member.span = peek().lexeme;
      member.left = std::make_unique<Expr>(std::move(expr));
      return member;
    }
    Expr member;
    member.kind = Expr::Kind::Member;
    member.scoped_member = scoped;
    member.value = std::string(peek().lexeme);
    member.span = peek().lexeme;
    member.location = peek().location;
    member.left = std::make_unique<Expr>(std::move(expr));
    advance();
    expr = std::move(member);
  }
  if (!expression_depth_valid(expr)) return std::nullopt;
  return expr;
}

}  // namespace clpp::parser
