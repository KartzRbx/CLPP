#include "tree_sitter/parser.h"

// Stateless scanner: comments use << and <<[ ... ]>> exactly as the compiler.
void *tree_sitter_clpp_external_scanner_create(void) { return NULL; }
void tree_sitter_clpp_external_scanner_destroy(void *payload) { (void)payload; }
unsigned tree_sitter_clpp_external_scanner_serialize(void *payload, char *buffer) {
  (void)payload; (void)buffer; return 0;
}
void tree_sitter_clpp_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  (void)payload; (void)buffer; (void)length;
}

bool tree_sitter_clpp_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  (void)payload;
  if (!valid_symbols[0]) return false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
    lexer->advance(lexer, true);
  }
  if (lexer->lookahead != '<') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '<') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '[') {
    while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = 0;
    return true;
  }
  lexer->advance(lexer, false);
  unsigned closing = 0;
  while (!lexer->eof(lexer)) {
    const int32_t character = lexer->lookahead;
    lexer->advance(lexer, false);
    if (character == ']') closing = 1;
    else if (character == '>' && closing > 0) {
      if (++closing == 3) {
        lexer->mark_end(lexer);
        lexer->result_symbol = 0;
        return true;
      }
    } else closing = 0;
  }
  return false;
}
