#pragma once

#include "clpp/core/lexer/token.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::parser {

struct Expr {
  enum class Kind {
    String,
    Number,
    Name,
    Concat,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    NotEq,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    And,
    Or,
    Not,
    Call,
    Member,
    Native,
    Construct,
    SelfField,
    Await,
    Spawn,
    Parallel,
    Index,
    PCall,
    MoveFrom,
    Join,
    Atomic,
    MakeList,
    Sort,
    Len,
    ListMut,  // push/pop/insert/remove on a variable: slot = the variable, number = 0 push, 1 pop, 2 insert, 3 remove
    Find,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    Shl,
    Shr,
    Range,
    Ternary,
    Update
  };

  Kind kind{Kind::String};
  std::string value;
  std::string_view span;
  SourceLocation location{};
  std::uint16_t slot{0};
  double number{0};
  bool float_literal{false};
  bool bool_literal{false};
  bool null_literal{false};
  bool enum_literal{false};
  bool variant_ctor{false};
  bool async_call{false};
  bool signal_call{false};
  bool thread_call{false};
  bool extern_call{false};
  bool int_specialized{false};
  bool bool_left{false};   // Concat: the left operand is a bool (joins as true/false)
  bool bool_right{false};  // Concat: the right operand is a bool
  bool virtual_call{false};
  bool missing{false};
  bool payload_ctor{false};
  std::uint8_t runtime_op{0};
  std::vector<std::uint16_t> targets;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
  std::vector<Expr> args;
  std::vector<std::string> arg_names;
  bool postfix{false};

  Expr();
  Expr(Expr&&) noexcept;
  Expr& operator=(Expr&&) noexcept;
  Expr(const Expr&) = delete;
  Expr& operator=(const Expr&) = delete;
  ~Expr();
};

struct Stmt {
  enum class Kind { Post, Let, Assign, If, While, For, ForIn, Break, Return, Expr, SelfAssign, Match, Signal, Connect, Using, Try, PlaceAssign };

  struct MatchArm {
    Expr pattern;
    Expr guard;
    std::vector<Stmt> body;
    bool wildcard{false};
    bool has_guard{false};
    bool has_payload{false};
    bool tag_match{false};
    std::string bind_name;
    std::uint16_t bind_slot{0};
  };

  Kind kind{Kind::Post};
  std::string name;
  std::string declared_type;
  std::string remembered;
  std::string_view name_span;
  SourceLocation name_location{};
  std::uint16_t slot{0};
  std::uint16_t limit_slot{0};
  std::uint16_t items_slot{0};   // for-in over a collection: the collection
  std::uint16_t index_slot{0};   // for-in over a collection: the position
  bool numeric_range{false};     // set by the type checker when the for-in source is a number (fast path)
  Expr expr;
  Expr place;  // PlaceAssign: the target, a chain of fields/indexes rooted at a variable (p.pos.x, xs[i], self.hp)
  std::vector<Stmt> then_body;
  std::vector<Stmt> else_body;
  std::vector<Stmt> init;
  std::vector<Stmt> step;
  std::vector<MatchArm> arms;
  bool has_else{false};
  bool immutable{false};
  bool returns_value{false};
  bool has_decltype{false};
  bool atomic_cell{false};
  bool observable_cell{false};
  bool if_let{false};
  bool print_as_bool{false};
  bool is_continue{false};  // Break statements: `continue;` instead of `break;`
  std::uint8_t channel{0};  // Post statements: 0 post/cout (stdout), 1 warn (stderr), 2 report (raises an error)  // set by the type checker: post(bool) prints true/false
  std::vector<std::string> unpack;
  std::vector<std::uint16_t> unpack_slots;
  std::vector<std::uint16_t> listeners;
  Expr type_expr;
};

struct Function {
  std::string name;
  std::string_view name_span;
  SourceLocation name_location{};
  std::string return_type;
  std::vector<std::string> params;
  std::vector<std::string> param_types;
  std::vector<std::string_view> param_spans;
  std::vector<SourceLocation> param_locations;
  std::vector<Stmt> body;
  std::uint16_t local_count{0};
  std::uint16_t index{0};
  bool is_async{false};
  bool is_extern{false};
  bool is_abstract{false};
  bool is_override{false};
  bool is_private{false};
  bool is_final{false};
  bool variadic{false};
  std::uint16_t extern_id{0};
  std::vector<std::string> type_params;
  std::vector<Expr> defaults;
  std::vector<char> has_default;
  std::vector<std::string> where_names;
  std::vector<std::string> where_types;
};

struct Field {
  std::string name;
  std::string type_name;
  std::string owner;
  std::string_view name_span;
  SourceLocation name_location{};
  bool is_private{false};
};

struct StructDecl {
  std::string name;
  std::string_view name_span;
  SourceLocation name_location{};
  std::string base;
  std::string_view base_span;
  SourceLocation base_location{};
  std::uint16_t base_index{65535};
  std::vector<std::string> bases;
  std::vector<std::string_view> base_spans;
  std::vector<SourceLocation> base_locations;
  std::vector<std::uint16_t> base_indices;
  std::vector<std::string> type_params;
  bool is_abstract{false};
  bool is_final{false};
  std::vector<Field> fields;
  std::vector<Function> methods;
  std::vector<std::string> method_names;
  std::vector<std::uint16_t> vtable;
  std::uint16_t index{0};
};

struct EnumDecl {
  std::string name;
  std::string_view name_span;
  SourceLocation name_location{};
  std::vector<std::string> variants;
  std::vector<std::string> payload_types;
  std::vector<std::string_view> variant_spans;
  std::vector<SourceLocation> variant_locations;
};

struct VariantDecl {
  std::string name;
  std::string_view name_span;
  SourceLocation name_location{};
  std::vector<std::string> alternatives;
};

struct LinkDecl {
  std::string path;
  std::string alias;
  std::string_view path_span;
  SourceLocation path_location{};
  std::string_view alias_span;
  SourceLocation alias_location{};
};

enum class CheckMode { Nonstrict, Strict, Nocheck };

struct TypeAlias {
  std::string name;
  std::string body;
};

struct Program {
  std::vector<StructDecl> structs;
  std::vector<EnumDecl> enums;
  std::vector<VariantDecl> variants;
  std::vector<Function> functions;
  std::vector<Stmt> statements;
  std::vector<LinkDecl> links;
  std::vector<TypeAlias> aliases;
  CheckMode mode{CheckMode::Nonstrict};
  std::uint16_t local_count{0};
};

}  // namespace clpp::parser
