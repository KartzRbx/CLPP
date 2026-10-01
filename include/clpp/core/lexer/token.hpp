#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace clpp {

struct SourceLocation {
  std::uint32_t line{1};
  std::uint32_t column{1};
};

enum class TokenType {
  Eof,
  Invalid,

  Identifier,
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  TemplateString,
  TemplateHead,
  TemplateMiddle,
  TemplateTail,

  // Types & values
  KwVoid,
  KwInt,
  KwFloat,
  KwDouble,
  KwString,
  KwBool,
  KwAuto,
  KwFunc,
  KwNull,
  KwTrue,
  KwType,
  KwFalse,

  // Declarations
  KwStruct,
  KwConst,
  KwStatic,
  KwConstexpr,
  KwNew,
  KwLink,
  KwAs,
  KwFrom,
  KwLet,
  KwMut,
  KwJoin,
  KwThread,
  KwAtomic,
  KwDecltype,
  KwMove,
  KwOperator,
  KwVariant,
  KwImport,
  KwUsing,
  KwEnum,
  KwExtern,
  KwAbstract,
  KwOverride,
  KwPrivate,

  // Control flow
  KwIf,
  KwElse,
  KwWhile,
  KwFor,
  KwIn,
  KwReturn,
  KwBreak,
  KwContinue,
  KwGuard,
  KwMatch,
  KwSwitch,
  KwCase,
  KwDefault,

  // Concurrency
  KwAsync,
  KwAwait,
  KwSpawn,
  KwParallel,
  KwTask,

  // Logic (CL++ uses and/or/not — not && ||)
  KwAnd,
  KwOr,
  KwNot,

  // Typed primitives (CL++ 0.8)
  KwObservable,
  KwSignal,

  // Builtin value types (Roblox / stdlib surface — lex as dedicated tokens)
  KwBuffer,
  KwVector2,
  KwVector3,
  KwVector4,

  // I/O
  KwPost,
  KwWarn,
  KwReport,
  KwCout,
  KwEndl,

  // Builtins often written as keywords in examples
  KwPcall,
  KwPublic,
  KwFinal,
  KwNamespace,
  KwWhere,
  KwTry,
  KwCatch,
  KwShl,
  KwShr,

  OpenParen,
  CloseParen,
  OpenBrace,
  CloseBrace,
  OpenBracket,
  CloseBracket,
  Semicolon,
  Comma,

  At,
  Hash,

  Plus,
  PlusPlus,
  Minus,
  MinusMinus,
  Star,
  Slash,
  Percent,

  PlusEq,
  MinusEq,
  StarEq,
  SlashEq,
  PercentEq,

  Equal,
  EqualEqual,
  NotEqual,
  Bang,
  Question,

  Less,
  LessEqual,
  Greater,
  GreaterEqual,

  Dot,
  DotDot,
  Ellipsis,
  Concat,
  Arrow,
  FatArrow,
  Colon,
  Scope,
  Pipe,
  Ampersand,
  Caret,
  BitNot,
  Mode,

  SignalConnect,

  DocComment,
};

struct Token {
  TokenType type{TokenType::Eof};
  std::string_view lexeme;
  SourceLocation location{};
};

struct Diagnostic {
  SourceLocation location{};
  // View into the analyzed source; valid only while that source buffer lives.
  std::string_view span;
  // Owned, so a diagnostic stays readable after the analysis that produced it is gone.
  std::string message;
};

struct LexResult {
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::string_view token_type_to_string(TokenType type);

}  // namespace clpp
