#pragma once

#include "core/parser/ast.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::module_types {
using Map = std::vector<parser::ResolvedSymbol>;
inline thread_local parser::Program* lexical_program = nullptr;
inline thread_local bool lexical_annotations = true;
inline thread_local bool lexical_globals = false;
struct LexicalScope {
  parser::Program* previous; bool annotations;
  LexicalScope(parser::Program& program, bool annotate) : previous(lexical_program), annotations(lexical_annotations) {
    lexical_program = &program; lexical_annotations = annotate;
  }
  ~LexicalScope() { lexical_program = previous; lexical_annotations = annotations; }
};
inline void statements(std::vector<parser::Stmt>&, const Map&, std::vector<std::string>, bool);

// Injective, deterministic encoding of the resolved loader path. The '$' prefix
// cannot be written as a source identifier, so private compiler names cannot leak
// into source lookup. Aliases of the same normalized path share one identity.
inline std::string prefix(const std::string_view path) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result = "$m";
  for (const unsigned char c : path) {
    result.push_back(hex[c >> 4]);
    result.push_back(hex[c & 15]);
  }
  return result + "_";
}

inline std::string resolve(const std::string& name, const Map& types) {
  for (const auto& type : types) if (type.spelling == name) return type.identity;
  return name;
}

inline std::string display(std::string text, const Map& types) {
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    if (text[cursor] == '"') {
      ++cursor;
      while (cursor < text.size() && text[cursor] != '"') {
        if (text[cursor] == '\\' && cursor + 1 < text.size()) ++cursor;
        ++cursor;
      }
      if (cursor < text.size()) ++cursor;
      continue;
    }
    if (text.compare(cursor, 2, "$m") != 0 && text.compare(cursor, 2, "$f") != 0) { ++cursor; continue; }
    std::size_t split = cursor + 2;
    while (split < text.size() && std::isxdigit(static_cast<unsigned char>(text[split])) != 0) ++split;
    if (split == cursor + 2 || split >= text.size() || text[split] != '_') { cursor += 2; continue; }
    std::size_t end = split + 1;
    while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) != 0 || text[end] == '_')) ++end;
    const auto identity = text.substr(cursor, end - cursor);
    auto replacement = text.substr(split + 1, end - split - 1);
    for (const auto& type : types) if (type.identity == identity) { replacement = type.spelling; break; }
    text.replace(cursor, end - cursor, replacement);
    cursor += replacement.size();
  }
  return text;
}

inline void annotation(std::string& text, const Map& types) {
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    if (text[cursor] == '"') {
      ++cursor;
      while (cursor < text.size() && text[cursor] != '"') {
        if (text[cursor] == '\\' && cursor + 1 < text.size()) ++cursor;
        ++cursor;
      }
      if (cursor < text.size()) ++cursor;
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(text[cursor])) == 0 && text[cursor] != '_' && text[cursor] != '$') {
      ++cursor;
      continue;
    }
    const std::size_t begin = cursor++;
    while (cursor < text.size() && (std::isalnum(static_cast<unsigned char>(text[cursor])) != 0 ||
           text[cursor] == '_' || text[cursor] == '.' || text[cursor] == '$')) ++cursor;
    const auto old = text.substr(begin, cursor - begin);
    const auto replacement = resolve(old, types);
    if (replacement != old) {
      text.replace(begin, cursor - begin, replacement);
      cursor = begin + replacement.size();
    }
  }
}

inline void expression(parser::Expr& expr, const Map& types, const std::vector<std::string>& locals = {}) {
  if (lexical_globals && expr.kind == parser::Expr::Kind::Call && !expr.left) {
    auto dot = expr.value.find('.');
    if (dot == std::string::npos) dot = expr.value.find("::");
    if (dot != std::string::npos) {
      const auto owner = expr.value.substr(0, dot);
      if (std::find(locals.begin(), locals.end(), owner) == locals.end()) {
        const auto resolved = resolve(owner, types);
        if (resolved != owner) expr.value = resolved + expr.value.substr(dot);
      }
    }
  }
  if (((expr.kind == parser::Expr::Kind::Call && expr.left == nullptr && expr.value.find("::") == std::string::npos) || expr.kind == parser::Expr::Kind::Construct || expr.kind == parser::Expr::Kind::Cast ||
       (expr.kind == parser::Expr::Kind::Event && expr.runtime_op == 0)) &&
      std::find(locals.begin(), locals.end(), expr.value) == locals.end()) {
    annotation(expr.value, types);
  } else if ((expr.kind == parser::Expr::Kind::Name || expr.kind == parser::Expr::Kind::Closure) && std::find(locals.begin(), locals.end(), expr.value) == locals.end()) {
    annotation(expr.value, types);
  }
  if (expr.kind == parser::Expr::Kind::Closure && lexical_program) {
    for (auto& nested : lexical_program->functions) {
      if (resolve(nested.name, types) != expr.value) continue;
      nested.lexical_names = locals;
      if (lexical_annotations) {
        annotation(nested.return_type, types);
        for (auto& type : nested.param_types) annotation(type, types);
      }
      auto scope = locals; scope.insert(scope.end(), nested.params.begin(), nested.params.end());
      statements(nested.body, types, std::move(scope), lexical_annotations);
      break;
    }
  }
  // A module-qualified type may be the receiver of a scoped enum case.
  if (expr.kind == parser::Expr::Kind::Member && !expr.scoped_member && expr.left != nullptr &&
      expr.left->kind == parser::Expr::Kind::Name &&
      std::find(locals.begin(), locals.end(), expr.left->value) == locals.end()) {
    const std::string source = expr.left->value + "." + expr.value;
    const std::string identity = resolve(source, types);
    if (identity != source) {
      expr.kind = parser::Expr::Kind::Name;
      expr.value = identity;
      expr.left.reset();
    }
  }
  for (auto& arg : expr.args) expression(arg, types, locals);
  if (expr.left != nullptr) expression(*expr.left, types, locals);
  if (expr.right != nullptr) expression(*expr.right, types, locals);
}

inline void statements(std::vector<parser::Stmt>& body, const Map& types, std::vector<std::string> locals = {},
                       const bool annotations = true) {
  for (auto& stmt : body) {
    if (lexical_globals && (stmt.kind == parser::Stmt::Kind::Assign || stmt.kind == parser::Stmt::Kind::PlaceAssign ||
        (stmt.kind == parser::Stmt::Kind::VariableDecl && stmt.module_global)) &&
        std::find(locals.begin(), locals.end(), stmt.name) == locals.end()) stmt.name = resolve(stmt.name, types);
    if (annotations && stmt.kind != parser::Stmt::Kind::Connect && stmt.kind != parser::Stmt::Kind::Using) {
      annotation(stmt.declared_type, types);
    } else if (!annotations && (stmt.kind == parser::Stmt::Kind::Connect || stmt.kind == parser::Stmt::Kind::Using)) {
      stmt.declared_type = resolve(stmt.declared_type, types);
    }
    auto children = locals;
    statements(stmt.init, types, children, annotations);
    for (const auto& init : stmt.init) if (init.kind == parser::Stmt::Kind::VariableDecl) children.push_back(init.name);
    expression(stmt.expr, types, children);
    expression(stmt.type_expr, types, children);
    expression(stmt.place, types, children);
    if (stmt.kind == parser::Stmt::Kind::ForIn) children.push_back(stmt.name);
    statements(stmt.then_body, types, children, annotations);
    statements(stmt.else_body, types, children, annotations);
    statements(stmt.step, types, children, annotations);
    for (auto& arm : stmt.arms) {
      expression(arm.pattern, types, locals);
      auto arm_locals = children;
      if (!arm.bind_name.empty()) arm_locals.push_back(arm.bind_name);
      expression(arm.guard, types, arm_locals);
      statements(arm.body, types, arm_locals, annotations);
    }
    if (stmt.kind == parser::Stmt::Kind::VariableDecl || stmt.kind == parser::Stmt::Kind::Signal) locals.push_back(stmt.name);
  }
}

inline Map without_parameters(Map types, const std::vector<std::string>& parameters) {
  types.erase(std::remove_if(types.begin(), types.end(), [&](const auto& type) {
    return std::find(parameters.begin(), parameters.end(), type.spelling) != parameters.end();
  }), types.end());
  return types;
}

inline void function(parser::Function& fn, const Map& all_types) {
  if (fn.anonymous) return;  // Signatures and bodies are rewritten in their lexical parent.
  const Map types = without_parameters(all_types, fn.type_params);
  const auto dot = fn.name.find('.');
  if (dot != std::string::npos) fn.name = resolve(fn.name.substr(0, dot), types) + fn.name.substr(dot);
  annotation(fn.return_type, types);
  for (auto& type : fn.param_types) annotation(type, types);
  for (auto& type : fn.where_types) annotation(type, types);
  if (!fn.anonymous) statements(fn.body, types, fn.params);
}

inline void program(parser::Program& program, const Map& types, const bool rename_declarations) {
  const LexicalScope lexical(program, true);
  statements(program.statements, types);
  for (auto& fn : program.functions) function(fn, types);
  for (auto& decl : program.structs) {
    const Map field_types = without_parameters(types, decl.type_params);
    if (rename_declarations) decl.name = resolve(decl.name, types);
    annotation(decl.base, field_types);
    for (auto& base : decl.bases) annotation(base, field_types);
    for (auto& field : decl.fields) {
      annotation(field.type_name, field_types);
      field.owner = resolve(field.owner, types);
      if (field.default_value != nullptr) expression(*field.default_value, field_types);
    }
    for (auto& fn : decl.methods) function(fn, field_types);
  }
  for (auto& decl : program.enums) {
    if (rename_declarations) decl.name = resolve(decl.name, types);
  }
  for (auto& decl : program.aliases) {
    if (rename_declarations) decl.name = resolve(decl.name, types);
    annotation(decl.body, without_parameters(types, decl.type_params));
  }
}
inline void global_references(parser::Program& program, const Map& globals) {
  const bool previous = lexical_globals; lexical_globals = true;
  struct Restore { bool value; ~Restore() { lexical_globals = value; } } restore{previous};
  const LexicalScope lexical(program, false);
  statements(program.statements, globals, {}, false);
  for (auto& fn : program.functions) if (!fn.anonymous) statements(fn.body, globals, fn.params, false);
  for (auto& data : program.structs) for (auto& field : data.fields)
    if (field.default_value) expression(*field.default_value, globals);
}
inline void function_references(parser::Program& program, const Map& functions, const bool rename_declarations) {
  const LexicalScope lexical(program, false);
  statements(program.statements, functions, {}, false);
  const auto rewrite = [&](parser::Function& fn) {
    if (rename_declarations) fn.name = resolve(fn.name, functions);
    if (!fn.anonymous) statements(fn.body, functions, fn.params, false);
  };
  for (auto& fn : program.functions) rewrite(fn);
  for (auto& decl : program.structs) {
    for (auto& field : decl.fields) if (field.default_value != nullptr) expression(*field.default_value, functions);
    for (auto& fn : decl.methods) rewrite(fn);
  }
}

}  // namespace clpp::module_types
