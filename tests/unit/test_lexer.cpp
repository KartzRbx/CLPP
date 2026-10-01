#include "clpp/core/lexer/lexer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <type_traits>
#include <string_view>

namespace {

void require_token(const clpp::Token& token, clpp::TokenType type, std::string_view lexeme) {
  REQUIRE(token.type == type);
  REQUIRE(token.lexeme == lexeme);
}

[[nodiscard]] std::size_t count_invalid(const std::vector<clpp::Token>& tokens) {
  std::size_t n = 0;
  for (const clpp::Token& t : tokens) {
    if (t.type == clpp::TokenType::Invalid) {
      ++n;
    }
  }
  return n;
}

void require_diagnostic(const clpp::Diagnostic& diagnostic, std::string_view message,
                        std::string_view span) {
  REQUIRE(diagnostic.message == message);
  REQUIRE(diagnostic.span == span);
}

}  // namespace

TEST_CASE("lexer empty source yields EOF", "[lexer]") {
  clpp::Lexer lexer("");
  const auto tokens = lexer.tokenize().tokens;
  REQUIRE(tokens.size() == 1);
  require_token(tokens[0], clpp::TokenType::Eof, "");
}

TEST_CASE("lexer comparison operators", "[lexer]") {
  clpp::Lexer lexer("!= < <= > >=");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::NotEqual, "!=");
  require_token(tokens[1], clpp::TokenType::Less, "<");
  require_token(tokens[2], clpp::TokenType::LessEqual, "<=");
  require_token(tokens[3], clpp::TokenType::Greater, ">");
  require_token(tokens[4], clpp::TokenType::GreaterEqual, ">=");
}

TEST_CASE("lexer CL++ line comment with <<", "[lexer]") {
  clpp::Lexer lexer("<< Esse é um comentario\npost");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwPost, "post");
}

TEST_CASE("lexer CL++ block comment multiline", "[lexer]") {
  const std::string source =
      "<<[\n"
      " Esse é um comentário com \n"
      "multiplas linhas \n"
      "]>>\n"
      "return";
  clpp::Lexer lexer(source);
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwReturn, "return");
}

TEST_CASE("lexer line then block comments like examples/comments.clp", "[lexer]") {
  const std::string source =
      "<< Esse é um comentario\n"
      "\n"
      "<<[\n"
      " Esse é um comentário com\n"
      " multiplas linhas\n"
      "]>>\n"
      "null";
  clpp::Lexer lexer(source);
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwNull, "null");
}

TEST_CASE("lexer doc comment token for LSP", "[lexer]") {
  clpp::Lexer lexer("<<[[]] Player health cap >>\nfunc");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::DocComment, " Player health cap ");
  require_token(tokens[1], clpp::TokenType::KwFunc, "func");
}

TEST_CASE("lexer numeric separators and scientific", "[lexer]") {
  clpp::Lexer lexer("10_000 10e5 1_500.50_01 2.5e-3");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::IntLiteral, "10_000");
  require_token(tokens[1], clpp::TokenType::FloatLiteral, "10e5");
  require_token(tokens[2], clpp::TokenType::FloatLiteral, "1_500.50_01");
  require_token(tokens[3], clpp::TokenType::FloatLiteral, "2.5e-3");
}

TEST_CASE("lexer hex and binary literals", "[lexer]") {
  clpp::Lexer lexer("0xFF_00_FF 0b1010_1100");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::IntLiteral, "0xFF_00_FF");
  require_token(tokens[1], clpp::TokenType::IntLiteral, "0b1010_1100");
}

TEST_CASE("lexer 3.Name splits int dot identifier", "[lexer]") {
  clpp::Lexer lexer("3.Name");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::IntLiteral, "3");
  require_token(tokens[1], clpp::TokenType::Dot, ".");
  require_token(tokens[2], clpp::TokenType::Identifier, "Name");
}

TEST_CASE("lexer double-quoted string literal", "[lexer]") {
  clpp::Lexer lexer(R"("hello CL++")");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::StringLiteral, "hello CL++");
}

TEST_CASE("lexer single-quoted string literal", "[lexer]") {
  clpp::Lexer lexer("'Kartz'");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::StringLiteral, "Kartz");
}

TEST_CASE("lexer backtick template string with interpolation", "[lexer]") {
  clpp::Lexer lexer("`Texto ${valor_do_text}`");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::TemplateHead, "Texto ");
  require_token(tokens[1], clpp::TokenType::Identifier, "valor_do_text");
  require_token(tokens[2], clpp::TokenType::TemplateTail, "");
  REQUIRE(tokens[3].type == clpp::TokenType::Eof);
}

TEST_CASE("lexer nested interpolation keeps inner braces", "[lexer]") {
  clpp::Lexer lexer("`a ${1 + {2}} b`");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::TemplateHead, "a ");
  require_token(tokens[1], clpp::TokenType::IntLiteral, "1");
  require_token(tokens[2], clpp::TokenType::Plus, "+");
  require_token(tokens[3], clpp::TokenType::OpenBrace, "{");
  require_token(tokens[4], clpp::TokenType::IntLiteral, "2");
  require_token(tokens[5], clpp::TokenType::CloseBrace, "}");
  require_token(tokens[6], clpp::TokenType::TemplateTail, " b");
}

TEST_CASE("lexer skips a utf-8 bom and reports a control character", "[lexer]") {
  const std::string bom = std::string("\xEF" "\xBB" "\xBF") + "let";
  clpp::Lexer marked(bom);
  const clpp::LexResult marked_result = marked.tokenize();
  require_token(marked_result.tokens[0], clpp::TokenType::KwLet, "let");
  REQUIRE(marked_result.tokens[0].location.line == 1);
  REQUIRE(marked_result.tokens[0].location.column == 1);
  REQUIRE(marked_result.diagnostics.empty());

  const std::string controls("a\x01\x7F", 3);
  clpp::Lexer raw(controls);
  const clpp::LexResult result = raw.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Identifier, "a");
  require_token(result.tokens[1], clpp::TokenType::Invalid, "\x01");
  require_token(result.tokens[2], clpp::TokenType::Invalid, "\x7F");
  REQUIRE(result.tokens[3].type == clpp::TokenType::Eof);
  REQUIRE(result.diagnostics.size() == 2);
  require_diagnostic(result.diagnostics[0], "control character", "\x01");
  require_diagnostic(result.diagnostics[1], "control character", "\x7F");
}

TEST_CASE("lexer keeps column across a line and recovers after an invalid token", "[lexer]") {
  const std::string source = "let\n  x\x01 = 1;";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::KwLet, "let");
  REQUIRE(result.tokens[0].location.column == 1);
  require_token(result.tokens[1], clpp::TokenType::Identifier, "x");
  REQUIRE(result.tokens[1].location.line == 2);
  REQUIRE(result.tokens[1].location.column == 3);
  require_token(result.tokens[2], clpp::TokenType::Invalid, "\x01");
  require_token(result.tokens[3], clpp::TokenType::Equal, "=");
  REQUIRE(result.tokens.back().type == clpp::TokenType::Eof);
}

TEST_CASE("lexer sigils for at-decorator and hash directives", "[lexer]") {
  clpp::Lexer lexer("@decorator\n#define\n#if\n#pragma");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::At, "@");
  require_token(tokens[1], clpp::TokenType::Identifier, "decorator");
  require_token(tokens[2], clpp::TokenType::Hash, "#");
  require_token(tokens[3], clpp::TokenType::Identifier, "define");
  require_token(tokens[4], clpp::TokenType::Hash, "#");
  require_token(tokens[5], clpp::TokenType::KwIf, "if");
  require_token(tokens[6], clpp::TokenType::Hash, "#");
  require_token(tokens[7], clpp::TokenType::Identifier, "pragma");
  REQUIRE(tokens.back().type == clpp::TokenType::Eof);
}

TEST_CASE("lexer escaped quote inside single-quoted string", "[lexer]") {
  clpp::Lexer lexer(R"('it\'s fine')");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::StringLiteral, "it\\'s fine");
}

TEST_CASE("lexer signal and concat operators", "[lexer]") {
  clpp::Lexer lexer(".: :: ~>");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::Concat, ".:");
  require_token(tokens[1], clpp::TokenType::Scope, "::");
  require_token(tokens[2], clpp::TokenType::SignalConnect, "~>");
}

TEST_CASE("lexer and or not keywords and compound ops", "[lexer]") {
  clpp::Lexer lexer("and or not += task::wait");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwAnd, "and");
  require_token(tokens[1], clpp::TokenType::KwOr, "or");
  require_token(tokens[2], clpp::TokenType::KwNot, "not");
  require_token(tokens[3], clpp::TokenType::PlusEq, "+=");
  require_token(tokens[4], clpp::TokenType::KwTask, "task");
  require_token(tokens[5], clpp::TokenType::Scope, "::");
  require_token(tokens[6], clpp::TokenType::Identifier, "wait");
}

TEST_CASE("lexer accepts plus-plus as one token", "[lexer]") {
  clpp::Lexer lexer("n++");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::Identifier, "n");
  require_token(tokens[1], clpp::TokenType::PlusPlus, "++");
}

TEST_CASE("lexer Vector3 math and concat chain", "[lexer]") {
  clpp::Lexer lexer("Vector3 nextPos = currentPos + (velocity * 0.016)");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwVector3, "Vector3");
  require_token(tokens[4], clpp::TokenType::Plus, "+");
  require_token(tokens[7], clpp::TokenType::Star, "*");
  require_token(tokens[8], clpp::TokenType::FloatLiteral, "0.016");
}

TEST_CASE("lexer buffer static scope calls", "[lexer]") {
  clpp::Lexer lexer("buffer stream = buffer::create(256)");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwBuffer, "buffer");
  require_token(tokens[1], clpp::TokenType::Identifier, "stream");
  require_token(tokens[2], clpp::TokenType::Equal, "=");
  require_token(tokens[3], clpp::TokenType::KwBuffer, "buffer");
  require_token(tokens[4], clpp::TokenType::Scope, "::");
  require_token(tokens[5], clpp::TokenType::Identifier, "create");
}

TEST_CASE("lexer link workspace at-path", "[lexer]") {
  clpp::Lexer lexer("link @clpp.roblox;");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwLink, "link");
  require_token(tokens[1], clpp::TokenType::At, "@");
  require_token(tokens[2], clpp::TokenType::Identifier, "clpp");
  require_token(tokens[3], clpp::TokenType::Dot, ".");
  require_token(tokens[4], clpp::TokenType::Identifier, "roblox");
}

TEST_CASE("lexer post with dot-colon and member access", "[lexer]") {
  clpp::Lexer lexer(R"(post("x" .: nextPos.x))");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwPost, "post");
  require_token(tokens[3], clpp::TokenType::Concat, ".:");
  require_token(tokens[4], clpp::TokenType::Identifier, "nextPos");
  require_token(tokens[5], clpp::TokenType::Dot, ".");
  require_token(tokens[6], clpp::TokenType::Identifier, "x");
}

TEST_CASE("lexer CL++ 0.8 control and concurrency keywords", "[lexer]") {
  clpp::Lexer lexer("guard async await spawn parallel while for in match");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwGuard, "guard");
  require_token(tokens[1], clpp::TokenType::KwAsync, "async");
  require_token(tokens[2], clpp::TokenType::KwAwait, "await");
  require_token(tokens[3], clpp::TokenType::KwSpawn, "spawn");
  require_token(tokens[4], clpp::TokenType::KwParallel, "parallel");
  require_token(tokens[5], clpp::TokenType::KwWhile, "while");
  require_token(tokens[6], clpp::TokenType::KwFor, "for");
  require_token(tokens[7], clpp::TokenType::KwIn, "in");
  require_token(tokens[8], clpp::TokenType::KwMatch, "match");
}

TEST_CASE("lexer self table @field @this.field @this::member", "[lexer]") {
  clpp::Lexer lexer("@coins @this.janitor @this::PlayerEntered");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::At, "@");
  require_token(tokens[1], clpp::TokenType::Identifier, "coins");
  require_token(tokens[2], clpp::TokenType::At, "@");
  require_token(tokens[3], clpp::TokenType::Identifier, "this");
  require_token(tokens[4], clpp::TokenType::Dot, ".");
  require_token(tokens[5], clpp::TokenType::Identifier, "janitor");
  require_token(tokens[6], clpp::TokenType::At, "@");
  require_token(tokens[7], clpp::TokenType::Identifier, "this");
  require_token(tokens[8], clpp::TokenType::Scope, "::");
  require_token(tokens[9], clpp::TokenType::Identifier, "PlayerEntered");
}

TEST_CASE("lexer indexing and pragma sigils", "[lexer]") {
  clpp::Lexer lexer("items[0] #pragma once @this");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::Identifier, "items");
  require_token(tokens[1], clpp::TokenType::OpenBracket, "[");
  require_token(tokens[2], clpp::TokenType::IntLiteral, "0");
  require_token(tokens[3], clpp::TokenType::CloseBracket, "]");
  require_token(tokens[4], clpp::TokenType::Hash, "#");
  require_token(tokens[5], clpp::TokenType::Identifier, "pragma");
  require_token(tokens[6], clpp::TokenType::Identifier, "once");
  require_token(tokens[7], clpp::TokenType::At, "@");
  require_token(tokens[8], clpp::TokenType::Identifier, "this");
}

TEST_CASE("lexer generic angle syntax GetService", "[lexer]") {
  clpp::Lexer lexer("GetService<Players>()");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::Identifier, "GetService");
  require_token(tokens[1], clpp::TokenType::Less, "<");
  require_token(tokens[2], clpp::TokenType::Identifier, "Players");
  require_token(tokens[3], clpp::TokenType::Greater, ">");
}

TEST_CASE("lexer link quoted path with as", "[lexer]") {
  clpp::Lexer lexer(R"(link "./Leader.clh" as Leader;)");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwLink, "link");
  require_token(tokens[1], clpp::TokenType::StringLiteral, "./Leader.clh");
  require_token(tokens[2], clpp::TokenType::KwAs, "as");
  require_token(tokens[3], clpp::TokenType::Identifier, "Leader");
}

TEST_CASE("lexer remaining compound assignments and bool literals", "[lexer]") {
  clpp::Lexer lexer("*= /= %= true false observable signal");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::StarEq, "*=");
  require_token(tokens[1], clpp::TokenType::SlashEq, "/=");
  require_token(tokens[2], clpp::TokenType::PercentEq, "%=");
  require_token(tokens[3], clpp::TokenType::KwTrue, "true");
  require_token(tokens[4], clpp::TokenType::KwFalse, "false");
  require_token(tokens[5], clpp::TokenType::KwObservable, "observable");
  require_token(tokens[6], clpp::TokenType::KwSignal, "signal");
}

TEST_CASE("lexer invalid hex without digits", "[lexer]") {
  clpp::Lexer lexer("0x 0");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "0x");
  require_token(result.tokens[1], clpp::TokenType::IntLiteral, "0");
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "invalid numeric literal", "0x");
}

TEST_CASE("lexer invalid float exponent without digits", "[lexer]") {
  clpp::Lexer lexer("1e+ x");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "1e+");
  require_token(result.tokens[1], clpp::TokenType::Identifier, "x");
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "invalid numeric literal", "1e+");
}

TEST_CASE("lexer line comment after incomplete doc opener", "[lexer]") {
  clpp::Lexer lexer("<<[ not a doc ]>>\nreturn");
  const auto tokens = lexer.tokenize().tokens;
  require_token(tokens[0], clpp::TokenType::KwReturn, "return");
}

TEST_CASE("lexer tokenizes examples/vectors_buffers.clp without invalid", "[lexer][integration]") {
  const std::string source = R"(link @clpp.roblox;

void UpdatePosition(Vector3 velocity) {
    Vector3 currentPos = Vector3(0.0, 10.0, 0.0);
    Vector3 nextPos = currentPos + (velocity * 0.016);
    << math nativa via opcode veloz

    post("New Pos: " .: nextPos.x .: ", " .: nextPos.y .: ", " .: nextPos.z);
}

void ProcessPacket() {
    buffer stream = buffer::create(256);
    buffer::write_string(stream, 0, "CL++ Data");

    post("Buffer allocated with size: " .: buffer::size(stream));
}

<< @coins, @this.janitor e @this::BindPart referem-se à mesma tabela (self)
)";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  REQUIRE(count_invalid(result.tokens) == 0);
  REQUIRE(result.diagnostics.empty());
  REQUIRE(result.tokens.back().type == clpp::TokenType::Eof);
}

TEST_CASE("lexer rejects bare exponent as one invalid literal", "[lexer]") {
  clpp::Lexer lexer("1e x");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "1e");
  require_token(result.tokens[1], clpp::TokenType::Identifier, "x");
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "invalid numeric literal", "1e");
}

TEST_CASE("lexer rejects numeric separators out of place", "[lexer]") {
  const std::string_view cases[] = {"10_", "1__0", "1_.5", "1._5", "1e_5", "0x_FF", "0xFF_", "0b_1"};
  for (const std::string_view source : cases) {
    clpp::Lexer lexer(source);
    const clpp::LexResult result = lexer.tokenize();
    require_token(result.tokens[0], clpp::TokenType::Invalid, source);
    REQUIRE(result.diagnostics.size() == 1);
    require_diagnostic(result.diagnostics[0], "invalid numeric separator", source);
  }
}

TEST_CASE("lexer unterminated string stops at newline", "[lexer]") {
  const std::string source = "\"hello\npost";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "\"hello");
  require_token(result.tokens[1], clpp::TokenType::KwPost, "post");
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "unterminated string", "\"hello");
  REQUIRE(result.tokens[0].lexeme.data() == source.data());
}

TEST_CASE("lexer unterminated template stops at newline", "[lexer]") {
  clpp::Lexer lexer("`hello\npost");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "`hello");
  require_token(result.tokens[1], clpp::TokenType::KwPost, "post");
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "unterminated template string", "`hello");
}

TEST_CASE("lexer unterminated string at eof", "[lexer]") {
  clpp::Lexer lexer("\"hello");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, "\"hello");
  REQUIRE(result.tokens[1].type == clpp::TokenType::Eof);
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "unterminated string", "\"hello");
}

TEST_CASE("lexer accepts and or sigils", "[lexer]") {
  clpp::Lexer lexer("a && b || c");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Identifier, "a");
  require_token(result.tokens[1], clpp::TokenType::KwAnd, "&&");
  require_token(result.tokens[2], clpp::TokenType::Identifier, "b");
  require_token(result.tokens[3], clpp::TokenType::KwOr, "||");
  require_token(result.tokens[4], clpp::TokenType::Identifier, "c");
  REQUIRE(result.diagnostics.empty());
}

TEST_CASE("lexer unexpected character lexeme points into source", "[lexer]") {
  const std::string source = "& ~";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Ampersand, "&");
  require_token(result.tokens[1], clpp::TokenType::BitNot, "~");
  REQUIRE(result.tokens[0].lexeme.data() == source.data());
  REQUIRE(result.tokens[1].lexeme.data() == source.data() + 2);
  REQUIRE(result.diagnostics.empty());
}

TEST_CASE("lexer unterminated block comment reports and yields eof", "[lexer]") {
  const std::string source = "<<[ never";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  REQUIRE(result.tokens.size() == 1);
  REQUIRE(result.tokens[0].type == clpp::TokenType::Eof);
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "unterminated block comment", source);
}

TEST_CASE("lexer unterminated doc comment is invalid", "[lexer]") {
  const std::string source = "<<[[]] hello";
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::Invalid, source);
  REQUIRE(result.tokens[1].type == clpp::TokenType::Eof);
  REQUIRE(result.diagnostics.size() == 1);
  require_diagnostic(result.diagnostics[0], "unterminated doc comment", source);
}

TEST_CASE("lexer crlf null and invalid utf-8 stay inside the process", "[lexer]") {
  clpp::Lexer lines("a\r\nb");
  const clpp::LexResult lined = lines.tokenize();
  REQUIRE(lined.tokens[1].lexeme == "b");
  REQUIRE(lined.tokens[1].location.line == 2);
  REQUIRE(lined.tokens[1].location.column == 1);

  const std::string null_source("a\0b", 3);
  clpp::Lexer nulls(null_source);
  const clpp::LexResult nulled = nulls.tokenize();
  REQUIRE_FALSE(nulled.diagnostics.empty());
  REQUIRE(nulled.tokens.back().type == clpp::TokenType::Eof);

  const std::string bad_utf8("\xff", 1);
  clpp::Lexer utf8(bad_utf8);
  const clpp::LexResult utf = utf8.tokenize();
  REQUIRE_FALSE(utf.diagnostics.empty());
  REQUIRE(utf.diagnostics[0].message == "invalid utf-8");
  REQUIRE(utf.tokens.back().type == clpp::TokenType::Eof);

  clpp::Lexer open("<<[ <<[ never");
  const clpp::LexResult comment = open.tokenize();
  REQUIRE(comment.diagnostics[0].message == "unterminated block comment");
  REQUIRE(comment.tokens.back().type == clpp::TokenType::Eof);
}

TEST_CASE("lexer accepts an accented identifier", "[lexer]") {
  clpp::Lexer lexer("café");
  const clpp::LexResult result = lexer.tokenize();
  REQUIRE(result.diagnostics.empty());
  require_token(result.tokens[0], clpp::TokenType::Identifier, "café");
  REQUIRE(result.tokens[1].type == clpp::TokenType::Eof);
}

TEST_CASE("lexer line comment at eof has no diagnostic", "[lexer]") {
  clpp::Lexer lexer("post << comment");
  const clpp::LexResult result = lexer.tokenize();
  require_token(result.tokens[0], clpp::TokenType::KwPost, "post");
  REQUIRE(result.tokens[1].type == clpp::TokenType::Eof);
  REQUIRE(result.diagnostics.empty());
}

TEST_CASE("line comment ends at LF, CRLF and a lone CR", "[lexer][crlf]") {
  for (const std::string source : {std::string("<< note\npost"), std::string("<< note\r\npost"),
                                   std::string("<< note\rpost")}) {
    clpp::Lexer lexer(source);
    const clpp::LexResult result = lexer.tokenize();
    REQUIRE(result.diagnostics.empty());
    REQUIRE(result.tokens.size() == 2);
    REQUIRE(result.tokens[0].type == clpp::TokenType::KwPost);
    REQUIRE(result.tokens[0].location.line == 2);
    REQUIRE(result.tokens[0].location.column == 1);
  }
}

TEST_CASE("block and doc comments span CRLF lines", "[lexer][crlf]") {
  const std::string block("<<[\r\n a\r\n]>>\r\npost");
  clpp::Lexer blocks(block);
  const clpp::LexResult b = blocks.tokenize();
  REQUIRE(b.diagnostics.empty());
  REQUIRE(b.tokens[0].type == clpp::TokenType::KwPost);
  REQUIRE(b.tokens[0].location.line == 4);
}

TEST_CASE("an unterminated string stops at CRLF and keeps the next line", "[lexer][crlf]") {
  const std::string source("post(\"open\r\npost");
  clpp::Lexer lexer(source);
  const clpp::LexResult result = lexer.tokenize();
  REQUIRE_FALSE(result.diagnostics.empty());
  REQUIRE(result.diagnostics[0].message == "unterminated string");
  REQUIRE(result.tokens[result.tokens.size() - 2].type == clpp::TokenType::KwPost);
  REQUIRE(result.tokens[result.tokens.size() - 2].location.line == 2);
}

TEST_CASE("a lexer cannot be built from a temporary string", "[lexer]") {
  STATIC_REQUIRE_FALSE(std::is_constructible_v<clpp::Lexer, std::string&&>);
  STATIC_REQUIRE(std::is_constructible_v<clpp::Lexer, const std::string&>);
}
